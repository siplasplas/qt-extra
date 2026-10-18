#include "qxmigration.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QVector>

namespace {

const QDir::Filters kAllEntries = QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot;

QString expandHome(const QString& path)
{
    if (path == QLatin1String("~"))
        return QDir::homePath();
    if (path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

QString displayPath(const QString& path)
{
    const QString home = QDir::homePath();
    if (path == home)
        return QStringLiteral("~");
    if (path.startsWith(home + QLatin1Char('/')))
        return QLatin1Char('~') + path.mid(home.size());
    return path;
}

bool pathExists(const QString& path)
{
    const QFileInfo info(path);
    return info.exists() || info.isSymLink();
}

void removeEntry(const QString& path)
{
    const QFileInfo info(path);
    if (info.isDir() && !info.isSymLink())
        QDir(path).removeRecursively();
    else
        QFile::remove(path);
}

bool copyEntry(const QString& from, const QString& to)
{
    const QFileInfo info(from);
    if (info.isSymLink())
        return QFile::link(info.symLinkTarget(), to);
    if (!info.isDir())
        return QFile::copy(from, to);
    if (!QDir().mkdir(to))
        return false;
    for (const QString& name : QDir(from).entryList(kAllEntries)) {
        if (!copyEntry(from + QLatin1Char('/') + name, to + QLatin1Char('/') + name))
            return false;
    }
    return true;
}

// Rename; across file systems copy, then remove the original after a
// successful copy. The target must not exist.
bool moveEntry(const QString& from, const QString& to)
{
    if (pathExists(to))
        return false;
    if (QDir().rename(from, to))
        return true;
    if (pathExists(to) || !copyEntry(from, to)) {
        if (!pathExists(from))
            return false;
        removeEntry(to);
        return false;
    }
    removeEntry(from);
    if (pathExists(from)) {
        // The original could not be removed: keep it and drop the copy.
        removeEntry(to);
        return false;
    }
    return true;
}

struct OldDirectory {
    QString path;
    bool config; // its unrecognized entries go to the new configuration directory
};

struct OldPlaces {
    QVector<OldDirectory> directories;
    QStringList settingsFiles;
};

OldPlaces oldPlaces(const QxMigrationSpec& spec)
{
    const QString app = QCoreApplication::applicationName();
    const QString genericConfig = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString genericData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    OldPlaces places;
    places.directories = {{genericConfig + QLatin1Char('/') + app, true},
                          {genericData + QLatin1Char('/') + app, false}};
    for (const QString& extra : spec.oldDirectories) {
        const QString path = QDir::cleanPath(expandHome(extra));
        bool known = false;
        for (const OldDirectory& dir : places.directories)
            known = known || dir.path == path;
        if (!known)
            places.directories.append({path, path.startsWith(genericConfig + QLatin1Char('/'))});
    }
    if (spec.oldSettingsFiles.isEmpty())
        places.settingsFiles << genericConfig + QLatin1Char('/') + app + QStringLiteral(".conf");
    for (const QString& file : spec.oldSettingsFiles)
        places.settingsFiles << QDir::cleanPath(expandHome(file));
    return places;
}

bool askWithMessageBox(const QxMigrationSpec& spec, const QStringList& unrecognized)
{
    // Without a GUI (command line, tests on the offscreen platform) the answer is No.
    if (!qobject_cast<QApplication*>(QCoreApplication::instance()))
        return false;
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))
        return false;

    const QString app = QGuiApplication::applicationDisplayName();
    const int shown = 10;
    QStringList names;
    for (int i = 0; i < unrecognized.size() && i < shown; ++i)
        names << displayPath(unrecognized.at(i));
    if (unrecognized.size() > shown)
        names << QObject::tr("… and %1 more").arg(unrecognized.size() - shown);
    const QString text = QObject::tr(
        "Found files %1 does not recognize in its old places:\n\n%2\n\n"
        "They may belong to an older version of %1 or to another program. "
        "Move them to %3 and %4?")
        .arg(app, names.join(QLatin1Char('\n')),
             displayPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)),
             displayPath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
    return QMessageBox::question(spec.parent, QObject::tr("Moving %1's files").arg(app), text,
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes;
}

} // namespace

QxMigrationResult qxMigrateToSiplasplas(const QxMigrationSpec& spec)
{
    QxMigrationResult result;
    auto log = [&result](const QString& line) { result.log << line; };

    const QString organization = QCoreApplication::organizationName();
    if (organization != QLatin1String("siplasplas")) {
        result.status = QxMigrationResult::Failed;
        log(QStringLiteral("Not migrated: the organization name is \"%1\", not \"siplasplas\"")
                .arg(organization));
        return result;
    }
    const QString newConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString newData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString newSettings = QSettings().fileName();
    if (newConfig.isEmpty() || newData.isEmpty()) {
        result.status = QxMigrationResult::Failed;
        log(QStringLiteral("Not migrated: no configuration or data location"));
        return result;
    }
    if (pathExists(newConfig) || pathExists(newData) || pathExists(newSettings))
        return result;

    const OldPlaces places = oldPlaces(spec);
    QVector<OldDirectory> directories;
    for (const OldDirectory& dir : places.directories) {
        if (QFileInfo(dir.path).isDir())
            directories << dir;
    }
    QStringList settingsFiles;
    for (const QString& file : places.settingsFiles) {
        if (QFileInfo(file).isFile())
            settingsFiles << file;
    }
    if (directories.isEmpty() && settingsFiles.isEmpty())
        return result;

    struct Move {
        QString from;
        QString to;
    };
    QVector<Move> moves;
    QVector<Move> unrecognizedMoves;
    QStringList unrecognized;
    QStringList kept;
    QSet<QString> targets;
    auto recognized = [&spec](const QString& path) { return !spec.recognize || spec.recognize(path); };
    auto plan = [&](QVector<Move>& list, const QString& from, const QString& to) {
        if (targets.contains(to)) {
            kept << from;
            log(QStringLiteral("Not moved %1: %2 comes from another old place")
                    .arg(displayPath(from), displayPath(to)));
            return;
        }
        targets.insert(to);
        list.append({from, to});
    };

    for (const QString& file : settingsFiles) {
        if (recognized(file)) {
            plan(moves, file, newSettings);
        } else {
            unrecognized << file;
            unrecognizedMoves.append({file, newSettings});
        }
    }
    for (const OldDirectory& dir : directories) {
        for (const QString& name : QDir(dir.path).entryList(kAllEntries, QDir::Name)) {
            const QString from = dir.path + QLatin1Char('/') + name;
            if (places.settingsFiles.contains(from))
                continue;
            const QString newName = spec.renames.value(name, name);
            const bool known = spec.configEntries.contains(name) || spec.dataEntries.contains(name)
                            || spec.discardedEntries.contains(name);
            if (!known || !recognized(from)) {
                unrecognized << from;
                unrecognizedMoves.append(
                    {from, (dir.config ? newConfig : newData) + QLatin1Char('/') + name});
            } else if (spec.configEntries.contains(name)) {
                plan(moves, from, newConfig + QLatin1Char('/') + newName);
            } else if (spec.dataEntries.contains(name)) {
                plan(moves, from, newData + QLatin1Char('/') + newName);
            }
            // Discarded entries stay until qxFinishMigration().
        }
    }

    if (!unrecognized.isEmpty()) {
        QStringList shown;
        for (const QString& path : unrecognized)
            shown << displayPath(path);
        const QString line = QStringLiteral("Not recognized in the old places: %1").arg(shown.join(QStringLiteral(", ")));
        qWarning("%s", qPrintable(line));
        log(line);
        const bool move = spec.askToMoveUnrecognized ? spec.askToMoveUnrecognized(unrecognized)
                                                     : askWithMessageBox(spec, unrecognized);
        if (move) {
            for (const Move& m : unrecognizedMoves)
                plan(moves, m.from, m.to);
        } else {
            kept << unrecognized;
            log(QStringLiteral("Left the files not recognized where they are"));
        }
    }

    QVector<Move> done;
    QStringList createdDirs;
    auto makeParents = [&createdDirs](const QString& path) {
        QStringList missing;
        for (QString dir = QFileInfo(path).absolutePath(); !pathExists(dir);
             dir = QFileInfo(dir).absolutePath())
            missing.prepend(dir);
        for (const QString& dir : missing) {
            if (!QDir().mkdir(dir))
                return false;
            createdDirs << dir;
        }
        return true;
    };
    for (const Move& m : moves) {
        if (pathExists(m.to)) {
            // Never overwrite; another instance may have moved it meanwhile.
            kept << m.from;
            log(QStringLiteral("Not moved %1: %2 exists").arg(displayPath(m.from), displayPath(m.to)));
            continue;
        }
        if (makeParents(m.to) && moveEntry(m.from, m.to)) {
            done << m;
            log(QStringLiteral("Moved %1 to %2").arg(displayPath(m.from), displayPath(m.to)));
            continue;
        }
        if (pathExists(m.to) && pathExists(m.from)) {
            kept << m.from;
            log(QStringLiteral("Not moved %1: %2 appeared meanwhile").arg(displayPath(m.from), displayPath(m.to)));
            continue;
        }
        // Roll back: the program uses the old places this time and tries again next time.
        log(QStringLiteral("Failed to move %1 to %2").arg(displayPath(m.from), displayPath(m.to)));
        for (int i = done.size() - 1; i >= 0; --i) {
            if (moveEntry(done.at(i).to, done.at(i).from))
                log(QStringLiteral("Moved back %1").arg(displayPath(done.at(i).from)));
            else
                log(QStringLiteral("Could not move back %1 from %2")
                        .arg(displayPath(done.at(i).from), displayPath(done.at(i).to)));
        }
        for (int i = createdDirs.size() - 1; i >= 0; --i)
            QDir().rmdir(createdDirs.at(i));
        result.status = QxMigrationResult::Failed;
        return result;
    }

    // The new places exist from now on, so the migration and its question
    // do not come back.
    QDir().mkpath(newConfig);
    QDir().mkpath(newData);

    bool left = !kept.isEmpty();
    for (const OldDirectory& dir : directories) {
        const QStringList rest = QDir(dir.path).entryList(kAllEntries, QDir::Name);
        if (rest.isEmpty() && QDir().rmdir(dir.path)) {
            log(QStringLiteral("Removed the empty %1").arg(displayPath(dir.path)));
        } else {
            left = true;
            log(QStringLiteral("Left %1, which still holds: %2")
                    .arg(displayPath(dir.path), rest.join(QStringLiteral(", "))));
        }
    }
    result.status = left ? QxMigrationResult::Partial : QxMigrationResult::Migrated;
    return result;
}

void qxFinishMigration(const QxMigrationSpec& spec)
{
    for (const OldDirectory& dir : oldPlaces(spec).directories) {
        if (!QFileInfo(dir.path).isDir())
            continue;
        for (const QString& name : spec.discardedEntries) {
            const QString path = dir.path + QLatin1Char('/') + name;
            if (pathExists(path))
                removeEntry(path);
        }
        if (QDir(dir.path).entryList(kAllEntries).isEmpty())
            QDir().rmdir(dir.path);
    }
}
