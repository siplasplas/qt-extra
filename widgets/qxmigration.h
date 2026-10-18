#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

/**
 * What a program recognizes in its old configuration and data places, for
 * qxMigrateToSiplasplas(). Entry names are relative to the old directories
 * (files or directories); a leading "~/" in paths stands for the home directory.
 */
struct QxMigrationSpec {
    /// Moved to AppConfigLocation, e.g. {"settings.json", "approvals.json"}.
    QStringList configEntries;
    /// Moved to AppLocalDataLocation, e.g. {"history.json"}.
    QStringList dataEntries;
    /// Recognized but not moved; qxFinishMigration() removes them once the
    /// program has made new ones (e.g. a Python virtual environment).
    QStringList discardedEntries;
    /// Old entry name -> new name, e.g. {"config.json", "recent-files.json"}.
    QHash<QString, QString> renames;
    /// Old QSettings files to become QSettings()' file, e.g.
    /// "~/.config/DiffMerge/DiffMerge.conf"; empty: <GenericConfigLocation>/<applicationName>.conf.
    QStringList oldSettingsFiles;
    /// Old directories besides <GenericConfigLocation>/<applicationName> and
    /// <GenericDataLocation>/<applicationName>.
    QStringList oldDirectories;
    /// Optional finer check of a recognized entry or settings file (a version
    /// field, a JSON key); false = not recognized.
    std::function<bool(const QString& path)> recognize;
    /// Asked when something in the old places is not recognized; true moves it
    /// too. Default: a question box, or No without a GUI (no QApplication, or
    /// the offscreen platform).
    std::function<bool(const QStringList& unrecognized)> askToMoveUnrecognized;
    /// Parent of the default question box.
    QWidget* parent = nullptr;
};

struct QxMigrationResult {
    enum Status {
        NotNeeded, ///< a new place exists already, or there is no old place
        Migrated,  ///< everything in the old places was moved
        Partial,   ///< moved, but something stayed in the old places (see log)
        Failed     ///< nothing changed; use the old places for this run
    };
    Status status = NotNeeded;
    QStringList log; ///< one line per action, for the program's own log
};

/**
 * Moves a program's files from its old places to the siplasplas ones, once,
 * at start-up before anything is read or created: configuration to
 * ~/.config/siplasplas/<app>/ (AppConfigLocation), data to
 * ~/.local/share/siplasplas/<app>/ (AppLocalDataLocation) and the old QSettings
 * file to QSettings()' file. Needs organizationName() "siplasplas" and the
 * program's applicationName().
 *
 * Does nothing when a new place exists already or no old place exists. Never
 * overwrites; on a failure everything moved in this run is moved back.
 * Unrecognized files in the old places are moved only when the user agrees.
 * Old directories are removed when empty.
 */
QxMigrationResult qxMigrateToSiplasplas(const QxMigrationSpec& spec);

/// After the program has created replacements for spec.discardedEntries:
/// removes them from the old directories, then the old directories when empty.
void qxFinishMigration(const QxMigrationSpec& spec);
