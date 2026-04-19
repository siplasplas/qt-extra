#pragma once
#include <QDialog>
#include <QStringList>

class QFileSystemModel;
class QTreeView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QToolButton;
class QModelIndex;

class QxFileBrowser : public QDialog
{
    Q_OBJECT
public:
    enum Mode     { Open, Save };
    enum SizeUnit { SizeSI = 1000, SizeIEC = 1024 };

    explicit QxFileBrowser(QWidget* parent = nullptr, Mode mode = Open);

    void    setDirectory(const QString& path);
    void    setNameFilter(const QString& filter);
    void    setDefaultSuffix(const QString& suffix);
    void    setSizeUnit(SizeUnit unit);
    QString selectedFile() const;

    static QString getOpenFileName(QWidget* parent,
                                   const QString& caption,
                                   const QString& dir,
                                   const QString& filter = QString());
    static QString getSaveFileName(QWidget* parent,
                                   const QString& caption,
                                   const QString& dir,
                                   const QString& filter = QString());

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onItemActivated(const QModelIndex& index);
    void onCurrentItemChanged(const QModelIndex& current);
    void onPathEditReturnPressed();
    void onFileEditReturnPressed();
    void onFilterChanged(int index);

private:
    void navigateTo(const QString& path, bool pushToHistory = true);
    void goBack();
    void goForward();
    void goUp();
    void updateNavButtons();
    void applyCurrentFilter();
    bool tryAccept();

    struct FilterEntry {
        QString     display;
        QStringList patterns;
    };
    QList<FilterEntry> parseFilter(const QString& filter);

    QFileSystemModel* m_model;
    QTreeView*        m_view;
    QLineEdit*        m_pathEdit;
    QLineEdit*        m_fileEdit;
    QComboBox*        m_filterCombo;
    QToolButton*      m_backBtn;
    QToolButton*      m_forwardBtn;
    QToolButton*      m_upBtn;
    QPushButton*      m_acceptBtn;

    Mode        m_mode;
    QString     m_currentPath;
    QString     m_defaultSuffix;
    QStringList m_history;
    int         m_historyPos = -1;
    QList<FilterEntry> m_filters;
};