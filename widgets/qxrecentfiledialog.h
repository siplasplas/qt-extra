#pragma once
#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;

/**
 * @class QxRecentFileDialog
 * @brief File open/save dialog with a built-in recently-used files list.
 *
 * The caller owns and persists the recent-files list; pass it in via
 * setRecentFiles() and read it back via recentFiles() after the dialog closes.
 * Static convenience methods mirror the QFileDialog API.
 */
class QxRecentFileDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { Open, Save };

    explicit QxRecentFileDialog(Mode mode, QWidget* parent = nullptr);

    void setRecentFiles(const QStringList& files);
    QStringList recentFiles() const { return m_recentFiles; }

    void setNameFilter(const QString& filter) { m_nameFilter = filter; }
    void setDefaultSuffix(const QString& suffix) { m_defaultSuffix = suffix; }
    void setInitialDirectory(const QString& dir) { m_directory = dir; }

    QString selectedFile() const { return m_selectedFile; }

    static QString getOpenFileName(QWidget* parent,
                                   const QString& title,
                                   const QString& dir,
                                   const QString& nameFilter,
                                   QStringList* recentFiles = nullptr);

    static QString getSaveFileName(QWidget* parent,
                                   const QString& title,
                                   const QString& dir,
                                   const QString& nameFilter,
                                   QStringList* recentFiles = nullptr);

private slots:
    void onBrowse();
    void onRecentItemClicked(QListWidgetItem* item);
    void onRecentItemDoubleClicked(QListWidgetItem* item);
    void onAccept();

private:
    void setupUi();
    void updateRecentList();
    void addToRecent(const QString& filePath);

    static constexpr int MAX_RECENT = 15;

    Mode        m_mode;
    QString     m_nameFilter;
    QString     m_defaultSuffix;
    QString     m_directory;
    QString     m_selectedFile;
    QStringList m_recentFiles;

    QListWidget* m_recentListWidget = nullptr;
    QLineEdit*   m_fileEdit         = nullptr;
    QPushButton* m_okButton         = nullptr;
};