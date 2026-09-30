#include "qxfilebreadcrumb.h"
#include "qxbreadcrumb.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenu>

namespace {

#ifdef Q_OS_WIN
constexpr bool kWindowsPaths = true;
#else
constexpr bool kWindowsPaths = false;
#endif

// Root of the given clean absolute path: "/" on Unix; "X:/" or "//server/share/" on Windows
QString rootOf(const QString& path)
{
    if (!kWindowsPaths)
        return QDir::rootPath();
    if (path.startsWith("//")) {
        const int share = path.indexOf('/', 2);
        const int end = share < 0 ? -1 : path.indexOf('/', share + 1);
        return end < 0 ? path + '/' : path.left(end + 1);
    }
    return path.left(2) + '/';
}

} // namespace

QxFileBreadcrumb::QxFileBreadcrumb(QWidget* parent)
    : QWidget(parent), m_breadcrumb(new QxBreadcrumb(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_breadcrumb);

    connect(m_breadcrumb, &QxBreadcrumb::segmentActivated, this, [this](int index) {
        if (index < 0 || index >= m_segmentPaths.size()) return;
        setPath(m_segmentPaths.at(index));
        emit pathActivated(m_path);
    });
    connect(m_breadcrumb, &QxBreadcrumb::menuRequested, this, [this](int index, QMenu* menu) {
        if (index < 0 || index >= m_segmentPaths.size()) return;
        if (kWindowsPaths && index == 0) {
            // First segment on Windows: switch between drives
            for (const QFileInfo& drive : QDir::drives()) {
                const QString drivePath = drive.absoluteFilePath();
                menu->addAction(QDir::toNativeSeparators(drivePath), this, [this, drivePath]() {
                    setPath(drivePath);
                    emit pathActivated(m_path);
                });
            }
            return;
        }
        const QDir parentDir(m_segmentPaths.at(index));
        const QFileInfoList children = parentDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& child : children) {
            const QString childPath = child.absoluteFilePath();
            menu->addAction(child.fileName(), this, [this, childPath]() {
                setPath(childPath);
                emit pathActivated(m_path);
            });
        }
    });
    setPath(QDir::currentPath());
}

void QxFileBreadcrumb::setPath(const QString& path)
{
    const QDir dir(path);
    if (!dir.exists()) return;
    m_path = QDir::cleanPath(dir.absolutePath());

    const QString root = rootOf(m_path);
    QString current = root;
    QStringList labels{QDir::toNativeSeparators(root)};
    m_segmentPaths = QStringList{root};
    for (const QString& part : m_path.mid(root.size()).split('/', Qt::SkipEmptyParts)) {
        current = QDir(current).filePath(part);
        labels.append(part);
        m_segmentPaths.append(current);
    }
    m_breadcrumb->setSegments(labels);
}
