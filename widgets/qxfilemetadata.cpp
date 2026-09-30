#include "qxfilemetadata.h"

#include <QFileSystemModel>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QImageReader>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>

namespace {
constexpr int NumericRole = Qt::UserRole + 713;
constexpr qint64 ScanLimit = 128 * 1024 * 1024;
using Cancel = std::shared_ptr<std::atomic_bool>;
quint32 le32(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return quint32(b[0]) | quint32(b[1]) << 8 | quint32(b[2]) << 16 | quint32(b[3]) << 24;
}
quint32 be32(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return quint32(b[3]) | quint32(b[2]) << 8 | quint32(b[1]) << 16 | quint32(b[0]) << 24;
}
quint16 le16(const char* p) {
    return quint8(p[0]) | quint16(quint8(p[1])) << 8;
}

double wavDuration(QFile& file, const Cancel& cancel) {
    const QByteArray head = file.read(12);
    if (head.size() != 12 || head.left(4) != "RIFF" || head.mid(8) != "WAVE") return -1;
    const qint64 end = qint64(le32(head.constData() + 4)) + 8;
    if (end > file.size() || end < 12) return -1;
    quint32 rate = 0, byteRate = 0;
    quint16 align = 0;
    qint64 bytes = -1;
    int chunks = 0;
    while (file.pos() + 8 <= end && !*cancel && ++chunks < 100000) {
        const QByteArray chunk = file.read(8);
        if (chunk.size() != 8) return -1;
        const quint32 size = le32(chunk.constData() + 4);
        const qint64 next = file.pos() + size + (size & 1);
        if (next > end) return -1;
        if (chunk.left(4) == "fmt ") {
            if (size < 16) return -1;
            const QByteArray fmt = file.read(qMin(size, quint32(40)));
            if (fmt.size() < 16) return -1;
            quint16 format = le16(fmt.constData());
            if (format == 0xfffe) {
                if (fmt.size() < 40 || le16(fmt.constData() + 16) < 22) return -1;
                static const char guidTail[] = {0, 0, 0, 0, 0x10, 0, char(0x80), 0, 0, char(0xaa), 0, 0x38, char(0x9b), 0x71};
                if (fmt.mid(26, 14) != QByteArray(guidTail, 14)) return -1;
                format = le16(fmt.constData() + 24);
            }
            if (format != 1 && format != 3) return -1; // PCM or IEEE float only
            const quint16 channels = le16(fmt.constData() + 2);
            rate = le32(fmt.constData() + 4);
            byteRate = le32(fmt.constData() + 8);
            align = le16(fmt.constData() + 12);
            const quint16 bits = le16(fmt.constData() + 14);
            if (!channels || !rate || !bits || bits % 8 ||
                quint64(channels) * (bits / 8) != align ||
                quint64(rate) * align != byteRate) return -1;
        } else if (chunk.left(4) == "data") {
            if (bytes < 0) bytes = 0;
            bytes += size;
        }
        if (!file.seek(next)) return -1;
    }
    if (*cancel || file.pos() != end || !byteRate || bytes <= 0 || bytes % align) return -1;
    return double(bytes) / byteRate;
}

struct Mp3Frame { int length = 0, rate = 0, samples = 0, version = 0, layer = 0; };
Mp3Frame mp3Frame(const QByteArray& bytes) {
    if (bytes.size() != 4) return {};
    const quint32 h = be32(bytes.constData());
    const int version = (h >> 19) & 3, layer = (h >> 17) & 3;
    const int bitrate = (h >> 12) & 15, sampleRate = (h >> 10) & 3;
    if ((h & 0xffe00000) != 0xffe00000 || version == 1 || !layer ||
        !bitrate || bitrate == 15 || sampleRate == 3 || (h & 3) == 2) return {};
    static const int rates[] = {44100, 48000, 32000};
    static const int v1[][14] = {
        {32,40,48,56,64,80,96,112,128,160,192,224,256,320},
        {32,48,56,64,80,96,112,128,160,192,224,256,320,384},
        {32,64,96,128,160,192,224,256,288,320,352,384,416,448}
    };
    static const int v2[][14] = {
        {8,16,24,32,40,48,56,64,80,96,112,128,144,160},
        {32,48,56,64,80,96,112,128,144,160,176,192,224,256}
    };
    const int rate = rates[sampleRate] / (version == 3 ? 1 : version == 2 ? 2 : 4);
    const int kbps = version == 3 ? v1[layer - 1][bitrate - 1]
                                 : v2[layer == 3 ? 1 : 0][bitrate - 1];
    const int padding = (h >> 9) & 1;
    const int samples = layer == 3 ? 384 : layer == 1 && version != 3 ? 576 : 1152;
    const int length = layer == 3 ? (12 * kbps * 1000 / rate + padding) * 4
                                 : (samples / 8 * kbps * 1000 / rate + padding);
    return {length, rate, samples, version, layer};
}

double mp3Duration(QFile& file, const Cancel& cancel) {
    qint64 end = file.size();
    // Inspect every frame, including VBR, rather than extrapolating from bitrate.
    if (end > ScanLimit) return -1;
    QByteArray tag = file.peek(10);
    if (tag.startsWith("ID3")) {
        if (tag.size() != 10 || quint8(tag[3]) < 2 || quint8(tag[3]) > 4) return -1;
        quint32 size = 0;
        for (int i = 6; i < 10; ++i) {
            if (quint8(tag[i]) & 128) return -1;
            size = (size << 7) | quint8(tag[i]);
        }
        const qint64 start = 10 + qint64(size) + (tag[3] == 4 && (tag[5] & 16) ? 10 : 0);
        if (start > end || !file.seek(start)) return -1;
    }
    const qint64 start = file.pos();
    if (end >= 128 && file.seek(end - 128) && file.read(3) == "TAG") end -= 128;
    if (end >= 32 && file.seek(end - 32)) {
        const QByteArray ape = file.read(32);
        if (ape.startsWith("APETAGEX")) {
            const quint32 size = le32(ape.constData() + 12);
            if (size < 32 || size > end - start) return -1;
            end -= size;
            if (end >= start + 32 && file.seek(end - 32) && file.read(8) == "APETAGEX") end -= 32;
        }
    }
    if (!file.seek(start)) return -1;
    Mp3Frame first;
    qint64 samples = 0;
    int frames = 0;
    quint32 declaredFrames = 0;
    bool xingFrame = false;
    while (file.pos() < end && !*cancel) {
        const qint64 pos = file.pos();
        const QByteArray header = file.read(4);
        const Mp3Frame frame = mp3Frame(header);
        if (!frame.length || pos + frame.length > end) return -1;
        if (!frames) {
            first = frame;
            if (frame.layer == 1) {
                const QByteArray payload = header + file.read(frame.length - 4);
                if (payload.size() != frame.length) return -1;
                const quint32 h = be32(header.constData());
                const bool mono = ((h >> 6) & 3) == 3;
                const int side = frame.version == 3 ? (mono ? 17 : 32) : (mono ? 9 : 17);
                const int offset = 4 + ((h & 0x10000) ? 0 : 2) + side;
                const QByteArray signature = payload.mid(offset, 4);
                if (signature == "Xing" || signature == "Info") {
                    if (payload.size() < offset + 8) return -1;
                    xingFrame = true;
                    if (be32(payload.constData() + offset + 4) & 1) {
                        if (payload.size() < offset + 12) return -1;
                        declaredFrames = be32(payload.constData() + offset + 8);
                        if (!declaredFrames) return -1;
                    }
                } else if (payload.mid(36, 4) == "VBRI") {
                    if (payload.size() < 54) return -1;
                    declaredFrames = be32(payload.constData() + 50);
                    if (!declaredFrames) return -1;
                }
            }
        }
        if (frame.rate != first.rate || frame.version != first.version || frame.layer != first.layer) return -1;
        samples += frame.samples;
        ++frames;
        if (!file.seek(pos + frame.length)) return -1;
    }
    const int audioFrames = frames - (xingFrame ? 1 : 0);
    if (*cancel || audioFrames < 2 || (declaredFrames && declaredFrames != quint32(audioFrames))) return -1;
    if (xingFrame) samples -= first.samples; // Xing/Info is metadata, not an audio frame.
    return double(samples) / first.rate;
}

quint32 oggCrc(const QByteArray& page) {
    quint32 crc = 0;
    for (int i = 0; i < page.size(); ++i) {
        crc ^= quint32(i >= 22 && i < 26 ? 0 : quint8(page[i])) << 24;
        for (int bit = 0; bit < 8; ++bit)
            crc = crc & 0x80000000 ? (crc << 1) ^ 0x04c11db7 : crc << 1;
    }
    return crc;
}

double oggDuration(QFile& file, const Cancel& cancel) {
    if (file.size() > ScanLimit) return -1;
    quint32 serial = 0, sequence = 0, rate = 0;
    quint64 granule = 0;
    int preSkip = 0;
    bool first = true, eos = false, continuation = false, identified = false;
    QByteArray packet;
    while (file.pos() < file.size() && !*cancel) {
        QByteArray page = file.read(27);
        if (page.size() != 27 || page.left(4) != "OggS" || page[4] != 0) return -1;
        const int flags = quint8(page[5]);
        const quint32 pageSerial = le32(page.constData() + 14);
        const quint32 pageSequence = le32(page.constData() + 18);
        if (flags & ~7 || eos || bool(flags & 1) != continuation) return -1;
        if (first) {
            if (!(flags & 2) || pageSequence != 0) return -1;
            serial = pageSerial;
        } else if ((flags & 2) || pageSerial != serial || pageSequence != sequence + 1) return -1;
        sequence = pageSequence;
        const QByteArray lacing = file.read(quint8(page[26]));
        if (lacing.size() != quint8(page[26])) return -1;
        int length = 0;
        for (char n : lacing) length += quint8(n);
        const QByteArray body = file.read(length);
        if (body.size() != length) return -1;
        page += lacing;
        page += body;
        if (oggCrc(page) != le32(page.constData() + 22)) return -1;
        if (!identified) {
            int offset = 0;
            for (char n : lacing) {
                packet += body.mid(offset, quint8(n));
                offset += quint8(n);
                if (packet.size() > 65536) return -1;
                if (quint8(n) < 255) {
                    if (packet.size() >= 30 && packet.left(7) == QByteArray("\1vorbis", 7)) {
                        if (le32(packet.constData() + 7) != 0 || !quint8(packet[11]) || !(packet[29] & 1)) return -1;
                        rate = le32(packet.constData() + 12);
                        const int small = quint8(packet[28]) & 15, large = quint8(packet[28]) >> 4;
                        if (small < 6 || large > 13 || small > large) return -1;
                    } else if (packet.size() >= 19 && packet.left(8) == "OpusHead") {
                        if (quint8(packet[8]) > 15 || !quint8(packet[9])) return -1;
                        rate = 48000;
                        preSkip = le16(packet.constData() + 10);
                    } else return -1;
                    identified = true;
                    break;
                }
            }
        }
        if (!lacing.isEmpty()) continuation = quint8(lacing.back()) == 255;
        const quint64 value = quint64(le32(page.constData() + 6)) |
                              quint64(le32(page.constData() + 10)) << 32;
        if (value != quint64(-1)) {
            if (value > quint64(std::numeric_limits<qint64>::max()) || value < granule) return -1;
            granule = value;
        }
        eos = flags & 4;
        if (eos && (continuation || value == quint64(-1))) return -1;
        first = false;
    }
    return !*cancel && eos && identified && rate && granule > quint64(preSkip)
        ? double(granule - preSkip) / rate : -1;
}

struct Result {
    QString path;
    qint64 size = 0, modified = 0;
    qint64 sourceSize = 0, sourceModified = 0;
    double duration = -1;
    QSize dimensions;
    bool retry = false;
};
struct Mailbox {
    QMutex mutex;
    QList<Result> results;
    std::atomic_int running{0};
};
class Job : public QRunnable {
public:
    std::function<void()> work;
    void run() override { work(); }
};
}

struct QxMetadataModel::State {
    bool audio = false, images = false, active = false;
    QString directory;
    QHash<QString, Result> cache;
    QSet<QString> pending;
    QSet<QString> imageSuffixes;
    QSet<QString> watched;
    Cancel cancel = std::make_shared<std::atomic_bool>(false);
    std::shared_ptr<Mailbox> mailbox = std::make_shared<Mailbox>();
    QTimer* timer = nullptr;
    QFileSystemWatcher* watcher = nullptr;
};

QxMetadataModel::QxMetadataModel(QObject* parent) : QIdentityProxyModel(parent), d(new State) {
    for (const QByteArray& format : QImageReader::supportedImageFormats())
        d->imageSuffixes.insert(QString::fromLatin1(format).toLower());
    d->timer = new QTimer(this);
    d->timer->setInterval(50);
    connect(d->timer, &QTimer::timeout, this, [this] { poll(); });
    d->watcher = new QFileSystemWatcher(this);
    connect(d->watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
        if (!d->active || (!d->audio && !d->images)) return;
        // Re-add on the next job, including after an atomic replacement/rename.
        d->watcher->removePath(path);
        d->watched.remove(path);
        d->cache.remove(path);
        d->pending.insert(path);
        auto* fs = static_cast<QFileSystemModel*>(sourceModel());
        const QModelIndex base = mapFromSource(fs->index(path));
        if (base.isValid()) emit dataChanged(base.siblingAtColumn(4), base.siblingAtColumn(6));
    });
}
QxMetadataModel::~QxMetadataModel() { *d->cancel = true; }
bool QxMetadataModel::audioEnabled() const { return d->audio; }
bool QxMetadataModel::imagesEnabled() const { return d->images; }
void QxMetadataModel::resetJobs() {
    *d->cancel = true;
    {
        QMutexLocker lock(&d->mailbox->mutex);
        d->mailbox->results.clear();
    }
    d->cancel = std::make_shared<std::atomic_bool>(false);
    // Retain the mailbox/running count so repeated toggles never flood the pool.
    d->pending.clear();
    d->cache.clear();
    if (!d->watcher->files().isEmpty()) d->watcher->removePaths(d->watcher->files());
    d->watched.clear();
    if (d->active && (d->audio || d->images)) d->timer->start();
    else d->timer->stop();
}
void QxMetadataModel::setFeatures(bool audio, bool images) {
    if (d->audio == audio && d->images == images) return;
    d->audio = audio;
    d->images = images;
    resetJobs();
    if (sourceModel()) {
        const QModelIndex root = mapFromSource(static_cast<QFileSystemModel*>(sourceModel())->index(d->directory));
        if (rowCount(root)) emit dataChanged(index(0, 4, root), index(rowCount(root) - 1, 6, root));
    }
}
void QxMetadataModel::setDirectory(const QString& path) { d->directory = path; resetJobs(); }
void QxMetadataModel::setActive(bool active) { d->active = active; resetJobs(); }
int QxMetadataModel::columnCount(const QModelIndex&) const { return 7; }
QModelIndex QxMetadataModel::index(int row, int column, const QModelIndex& parent) const {
    if (column < 0 || column >= 7) return {};
    if (column < 4) return QIdentityProxyModel::index(row, column, parent);
    const QModelIndex base = QIdentityProxyModel::index(row, 0, parent);
    return base.isValid() ? createIndex(row, column, base.internalPointer()) : QModelIndex{};
}
QModelIndex QxMetadataModel::sibling(int row, int column, const QModelIndex& item) const {
    return index(row, column, item.parent());
}
QModelIndex QxMetadataModel::mapToSource(const QModelIndex& item) const {
    if (item.isValid() && item.column() >= 4)
        return QIdentityProxyModel::mapToSource(createIndex(item.row(), 0, item.internalPointer()));
    return QIdentityProxyModel::mapToSource(item);
}
Qt::ItemFlags QxMetadataModel::flags(const QModelIndex& item) const {
    const auto base = QIdentityProxyModel::flags(item);
    return item.column() >= 4 ? base & ~Qt::ItemIsEditable : base;
}
QVariant QxMetadataModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && section >= 4) {
        if (role != Qt::DisplayRole) return {};
        return section == 4 ? "Duration" : section == 5 ? "Width" : "Height";
    }
    return QIdentityProxyModel::headerData(section, orientation, role);
}
QVariant QxMetadataModel::data(const QModelIndex& item, int role) const {
    if (item.column() < 4) return QIdentityProxyModel::data(item, role);
    if (role == Qt::TextAlignmentRole) return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role != Qt::DisplayRole && role != NumericRole) return {};
    if ((item.column() == 4 ? !d->audio : !d->images) || !d->active) return {};
    auto* fs = static_cast<QFileSystemModel*>(sourceModel());
    const QFileInfo info = fs->fileInfo(mapToSource(item));
    if (info.isDir() || info.absolutePath() != d->directory) return {};
    const QString path = info.absoluteFilePath();
    auto found = d->cache.constFind(path);
    if (found == d->cache.constEnd() || found->sourceSize != info.size() ||
        found->sourceModified != info.lastModified().toMSecsSinceEpoch()) {
        d->pending.insert(path);
        return {};
    }
    if (item.column() == 4) {
        if (found->duration < 0) return {};
        if (role == NumericRole) return found->duration;
        const qint64 tenths = qint64(std::floor(found->duration * 10 + 0.5));
        return QString("%1:%2.%3").arg(tenths / 600, 2, 10, QLatin1Char('0'))
            .arg((tenths / 10) % 60, 2, 10, QLatin1Char('0')).arg(tenths % 10);
    }
    if (!found->dimensions.isValid() || found->dimensions.isEmpty()) return {};
    return item.column() == 5 ? found->dimensions.width() : found->dimensions.height();
}

void QxMetadataModel::poll() {
    QList<Result> results;
    {
        QMutexLocker lock(&d->mailbox->mutex);
        results.swap(d->mailbox->results);
    }
    auto* fs = static_cast<QFileSystemModel*>(sourceModel());
    for (const Result& result : results) {
        if (QFileInfo(result.path).absolutePath() != d->directory) continue;
        if (result.retry) {
            d->cache.remove(result.path);
            d->pending.insert(result.path);
        } else d->cache.insert(result.path, result);
        const QModelIndex base = mapFromSource(fs->index(result.path));
        if (base.isValid()) emit dataChanged(base.siblingAtColumn(4), base.siblingAtColumn(6));
    }
    while (!d->pending.isEmpty() && d->mailbox->running < 2) {
        const QString path = *d->pending.constBegin();
        d->pending.remove(path);
        const QFileInfo info = fs->fileInfo(fs->index(path));
        if (info.isDir() || info.absolutePath() != d->directory) continue;
        const Cancel cancel = d->cancel;
        const auto mailbox = d->mailbox;
        const QString suffix = info.suffix().toLower();
        const bool audio = d->audio && (suffix == "wav" || suffix == "mp3" || suffix == "ogg" || suffix == "oga" || suffix == "opus");
        const bool images = d->images && d->imageSuffixes.contains(suffix);
        Result result;
        result.path = path;
        result.size = info.size();
        result.modified = info.lastModified().toMSecsSinceEpoch();
        result.sourceSize = result.size;
        result.sourceModified = result.modified;
        // Mark in flight to avoid repeated requests during sorting/repainting.
        d->cache.insert(path, result);
        if (!audio && !images) continue;
        if (!d->watched.contains(path) && d->watcher->addPath(path)) d->watched.insert(path);
        ++mailbox->running;
        auto* job = new Job;
        job->work = [cancel, mailbox, result, audio, images]() mutable {
            if (!*cancel) {
                const QFileInfo before(result.path);
                result.size = before.size();
                result.modified = before.lastModified().toMSecsSinceEpoch();
                const QString suffix = QFileInfo(result.path).suffix().toLower();
                if (audio) {
                    QFile file(result.path);
                    if (!*cancel && file.open(QIODevice::ReadOnly)) {
                        if (suffix == "wav") result.duration = wavDuration(file, cancel);
                        else if (suffix == "mp3") result.duration = mp3Duration(file, cancel);
                        else result.duration = oggDuration(file, cancel);
                    }
                }
                if (images && !*cancel) {
                    QImageReader reader(result.path);
                    reader.setAutoTransform(false); // Encoded pixel dimensions, before EXIF rotation.
                    result.dimensions = reader.size();
                }
                const QFileInfo after(result.path);
                result.retry = after.size() != result.size || after.lastModified().toMSecsSinceEpoch() != result.modified;
                if (!*cancel) {
                    QMutexLocker lock(&mailbox->mutex);
                    if (!*cancel) mailbox->results.append(result);
                }
            }
            --mailbox->running;
        };
        QThreadPool::globalInstance()->start(job);
    }
}

bool QxMetadataSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    auto* metadata = static_cast<QxMetadataModel*>(sourceModel());
    auto* fs = static_cast<QFileSystemModel*>(metadata->sourceModel());
    const QModelIndex a = metadata->mapToSource(left), b = metadata->mapToSource(right);
    const bool dirA = fs->isDir(a), dirB = fs->isDir(b);
    if (dirA != dirB) return sortOrder() == Qt::AscendingOrder ? dirA : dirB;
    if (left.column() >= 4) {
        const QVariant av = left.data(NumericRole), bv = right.data(NumericRole);
        if (av.isValid() != bv.isValid())
            return sortOrder() == Qt::AscendingOrder ? av.isValid() : bv.isValid();
        if (av.isValid() && av.toDouble() != bv.toDouble()) return av.toDouble() < bv.toDouble();
    } else if (left.column() == 1 && !dirA) {
        if (fs->size(a) != fs->size(b)) return fs->size(a) < fs->size(b);
    } else if (left.column() == 3) {
        return fs->fileInfo(a).lastModified() < fs->fileInfo(b).lastModified();
    } else if (left.column() != 0) return QSortFilterProxyModel::lessThan(left, right);
    return QString::localeAwareCompare(fs->fileName(a), fs->fileName(b)) < 0;
}
