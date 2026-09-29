#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QLineEdit;

/** Dialog for files or directories with a caller-owned list of recent paths. */
class QxRecentDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { Open, Save, Directory };

    explicit QxRecentDialog(Mode mode, QWidget* parent = nullptr);

    void setRecentPaths(const QStringList& paths);
    QStringList recentPaths() const { return m_recentPaths; }

    void setNameFilter(const QString& filter) { m_nameFilter = filter; }
    void setDefaultSuffix(const QString& suffix) { m_defaultSuffix = suffix; }
    void setInitialDirectory(const QString& dir) { m_directory = dir; }

    QString selectedPath() const { return m_selectedPath; }

    static QString getOpenFileName(QWidget* parent, const QString& title,
                                   const QString& dir, const QString& nameFilter,
                                   QStringList* recentPaths = nullptr);
    static QString getSaveFileName(QWidget* parent, const QString& title,
                                   const QString& dir, const QString& nameFilter,
                                   QStringList* recentPaths = nullptr);
    static QString getExistingDirectory(QWidget* parent, const QString& title,
                                        const QString& dir,
                                        QStringList* recentPaths = nullptr);

private slots:
    void onBrowse();
    void onRecentItemClicked(QListWidgetItem* item);
    void onRecentItemDoubleClicked(QListWidgetItem* item);
    void onAccept();

private:
    void setupUi();
    void updateRecentList();
    void addToRecent(const QString& path);

    static constexpr int MAX_RECENT = 15;

    Mode        m_mode;
    QString     m_nameFilter;
    QString     m_defaultSuffix;
    QString     m_directory;
    QString     m_selectedPath;
    QStringList m_recentPaths;

    QListWidget* m_recentListWidget = nullptr;
    QLineEdit*   m_pathEdit         = nullptr;
};
