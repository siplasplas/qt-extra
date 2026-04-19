#pragma once
#include <QDialog>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;

/**
 * @class QxDirDialog
 * @brief Directory selection dialog with a built-in recently-used directories list.
 *
 * The caller owns and persists the recent-dirs list; pass it in via
 * setRecentDirs() and read it back via recentDirs() after the dialog closes.
 */
class QxDirDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QxDirDialog(QWidget* parent = nullptr);

    void setRecentDirs(const QStringList& dirs);
    QStringList recentDirs() const { return m_recentDirs; }

    void setInitialDirectory(const QString& dir) { m_directory = dir; }

    QString selectedDir() const { return m_selectedDir; }

    static QString getExistingDirectory(QWidget* parent,
                                        const QString& title,
                                        const QString& dir,
                                        QStringList* recentDirs = nullptr);

private slots:
    void onBrowse();
    void onRecentItemClicked(QListWidgetItem* item);
    void onRecentItemDoubleClicked(QListWidgetItem* item);
    void onAccept();

private:
    void setupUi();
    void updateRecentList();
    void addToRecent(const QString& dirPath);

    static constexpr int MAX_RECENT = 15;

    QString     m_directory;
    QString     m_selectedDir;
    QStringList m_recentDirs;

    QListWidget* m_recentListWidget = nullptr;
    QLineEdit*   m_dirEdit          = nullptr;
    QPushButton* m_okButton         = nullptr;
};