#pragma once
#include <QDialog>
#include <QStringList>

class QFileSystemModel;
class QListWidget;
class QListWidgetItem;
class QTreeView;
class QLineEdit;
class QComboBox;
class QPushButton;
class QToolButton;
class QModelIndex;
class QPoint;
class QxFileBreadcrumb;
class QxMetadataModel;
class QSortFilterProxyModel;

class QxFileDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode     { Open, Save, Directory };
    enum SizeUnit { SizeSI = 1000, SizeIEC = 1024 };

    explicit QxFileDialog(QWidget* parent = nullptr, Mode mode = Open);

    void    setDirectory(const QString& path);
    QString directory() const;
    void    setAudioDurationVisible(bool visible);
    void    setImageDimensionsVisible(bool visible);
    void    setNameFilter(const QString& filter);
    void    setDefaultSuffix(const QString& suffix);
    void    setSizeUnit(SizeUnit unit);
    void    setFileName(const QString& name);
    void    setHistory(const QStringList& paths);
    QString selectedFile() const;

    static QString getOpenFileName(QWidget* parent,
                                   const QString& caption,
                                   const QString& dir,
                                   const QString& filter = {},
                                   const QStringList& history = {});
    static QString getSaveFileName(QWidget* parent,
                                   const QString& caption,
                                   const QString& dir,
                                   const QString& filter = {},
                                   const QString& defaultName = {},
                                   const QStringList& history = {});
    static QString getExistingDirectory(QWidget* parent,
                                        const QString& caption,
                                        const QString& dir,
                                        const QStringList& history = {});

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onItemActivated(const QModelIndex& index);
    void onCurrentItemChanged(const QModelIndex& current);
    void onViewContextMenu(const QPoint& pos);
    void onFilterChanged(int index);

private:
    void navigateTo(const QString& path, bool pushToHistory = true);
    void createFolder();
    void goBack();
    void goForward();
    void goUp();
    void updateNavButtons();
    void applyCurrentFilter();
    bool consumeTypedPath();
    bool tryAccept();

    struct FilterEntry {
        QString     display;
        QStringList patterns;
    };
    QList<FilterEntry> parseFilter(const QString& filter);

    QFileSystemModel* m_model;
    QxMetadataModel*   m_metadata;
    QSortFilterProxyModel* m_proxy;
    QListWidget*      m_places;
    QTreeView*        m_view;
    QxFileBreadcrumb* m_breadcrumb;
    QComboBox*        m_fileEdit;
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
    bool        m_nameFromSelection = false;  // file edit filled from the view, not typed
    QList<FilterEntry> m_filters;
};
