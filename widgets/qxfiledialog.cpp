#include "qxfiledialog.h"
#include "qxfilebreadcrumb.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeView>
#include <QListWidget>
#include <QSplitter>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QFileSystemModel>
#include <QSet>
#include <QStandardPaths>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QDateTime>
#include <QStorageInfo>

#include <algorithm>

class DateFileSystemModel : public QFileSystemModel {
    int m_sizeBase = 1000;
public:
    using QFileSystemModel::QFileSystemModel;
    void setSizeBase(int base) { m_sizeBase = base; }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole) {
            if (index.column() == 3)
                return fileInfo(index).lastModified().toString("yyyy-MM-dd HH:mm");
            if (index.column() == 1 && !isDir(index))
                return formatSize(fileInfo(index).size(), m_sizeBase);
        }
        return QFileSystemModel::data(index, role);
    }

private:
    static QString formatSize(qint64 bytes, int base) {
        if (bytes < base)
            return QString::number(bytes) + " B";
        const char* s1000[] = { "kB", "MB", "GB", "TB", "PB" };
        const char* s1024[] = { "KiB", "MiB", "GiB", "TiB", "PiB" };
        const char** s = (base == 1024) ? s1024 : s1000;
        double v = bytes;
        int i = 0;
        while (v >= base && i < 4) { v /= base; ++i; }
        QString n = (v < 10)  ? QString::number(v, 'f', 2)
                  : (v < 100) ? QString::number(v, 'f', 1)
                  :              QString::number(static_cast<int>(v + 0.5));
        return n + ' ' + s[i - 1];
    }
};

// Commits in-place renames and reports when the file system refuses them
class RenameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override {
        const QString oldName = index.data(Qt::EditRole).toString();
        const QString newName = qobject_cast<QLineEdit*>(editor)->text().trimmed();
        if (newName.isEmpty() || newName == oldName)
            return;
        if (!model->setData(index, newName, Qt::EditRole))
            QMessageBox::warning(editor->window(), "Rename",
                QString("Cannot rename \"%1\" to \"%2\".\n"
                        "The name may be invalid or already in use.").arg(oldName, newName));
    }
};

QxFileDialog::QxFileDialog(QWidget* parent, Mode mode)
    : QDialog(parent), m_mode(mode)
{
    setWindowTitle(mode == Open ? "Open File" : mode == Save ? "Save File" : "Select Directory");
    resize(720, 520);

    // Model — watch full filesystem; view root index will select the directory
    m_model = new DateFileSystemModel(this);
    m_model->setRootPath(QDir::rootPath());
    m_model->setReadOnly(false);  // allow in-place rename
    m_model->setFilter(mode == Directory
        ? QDir::Dirs  | QDir::NoDotAndDotDot
        : QDir::AllEntries | QDir::AllDirs | QDir::NoDotAndDotDot);

    // Places panel (left)
    m_places = new QListWidget;
    m_places->setFrameShape(QFrame::NoFrame);
    m_places->setMinimumWidth(80);
    m_places->setMaximumWidth(260);
    m_places->setSpacing(1);

    struct Place { QString name; QStandardPaths::StandardLocation loc; const char* themeIcon; };
    static const Place places[] = {
        { "Home",        QStandardPaths::HomeLocation,           "user-home"           },
        { "Desktop",     QStandardPaths::DesktopLocation,        "user-desktop"        },
        { "Documents",   QStandardPaths::DocumentsLocation,      "folder-documents"    },
        { "Downloads",   QStandardPaths::DownloadLocation,       "folder-download"     },
        { "Music",       QStandardPaths::MusicLocation,          "folder-music"        },
        { "Pictures",    QStandardPaths::PicturesLocation,       "folder-pictures"     },
        { "Videos",      QStandardPaths::MoviesLocation,         "folder-videos"       },
        { "Fonts",       QStandardPaths::FontsLocation,          "folder"              },
        { "Config",      QStandardPaths::GenericConfigLocation,  "folder-settings"     },
        { "Data",        QStandardPaths::GenericDataLocation,    "folder"              },
        { "Cache",       QStandardPaths::GenericCacheLocation,   "folder"              },
        { "Apps",        QStandardPaths::ApplicationsLocation,   "folder-applications" },
        { "Temp",        QStandardPaths::TempLocation,           "folder-temp"         },
    };
    QSet<QString> seenPaths;
    for (const auto& p : places) {
        QString path = QStandardPaths::writableLocation(p.loc);
        if (path.isEmpty() || !QDir(path).exists() || seenPaths.contains(path)) continue;
        seenPaths.insert(path);
        auto* item = new QListWidgetItem(p.name, m_places);
        QIcon icon = QIcon::fromTheme(p.themeIcon);
        if (icon.isNull()) icon = style()->standardIcon(QStyle::SP_DirIcon);
        item->setIcon(icon);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }

    // File tree view (multi-column)
    m_view = new QTreeView;
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setColumnHidden(1, mode == Directory); // hide Size in dir mode
    m_view->setColumnHidden(2, true);              // hide "Type" column
    m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_view->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setEditTriggers(QAbstractItemView::EditKeyPressed);  // F2; double-click still activates
    m_view->setItemDelegate(new RenameDelegate(m_view));

    // Navigation bar
    m_backBtn    = new QToolButton; m_backBtn->setText("←");
    m_forwardBtn = new QToolButton; m_forwardBtn->setText("→");
    m_upBtn      = new QToolButton; m_upBtn->setText("↑");
    m_breadcrumb = new QxFileBreadcrumb;
    m_breadcrumb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* navLayout = new QHBoxLayout;
    navLayout->addWidget(m_backBtn);
    navLayout->addWidget(m_forwardBtn);
    navLayout->addWidget(m_upBtn);
    navLayout->addWidget(m_breadcrumb, 1);

    // Bottom bar
    m_fileEdit = new QComboBox;
    m_fileEdit->setEditable(true);
    m_fileEdit->setInsertPolicy(QComboBox::NoInsert);
    m_fileEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_filterCombo = new QComboBox;
    m_filterCombo->setMinimumWidth(200);
    m_acceptBtn   = new QPushButton(mode == Open ? "Open" : mode == Save ? "Save" : "Choose");
    auto* cancelBtn = new QPushButton("Cancel");
    m_acceptBtn->setDefault(true);

    auto* bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(new QLabel(mode == Directory ? "Dir name:" : "File name:"));
    bottomLayout->addWidget(m_fileEdit, 1);
    if (mode != Directory)
        bottomLayout->addWidget(m_filterCombo);
    else
        m_filterCombo->hide();

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_acceptBtn);
    btnLayout->addWidget(cancelBtn);

    auto* centerSplitter = new QSplitter(Qt::Horizontal);
    centerSplitter->addWidget(m_places);
    centerSplitter->addWidget(m_view);
    centerSplitter->setStretchFactor(0, 0);
    centerSplitter->setStretchFactor(1, 1);
    centerSplitter->setChildrenCollapsible(false);
    centerSplitter->setSizes({150, 530});

    // Main layout
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(navLayout);
    mainLayout->addWidget(centerSplitter, 1);
    mainLayout->addLayout(bottomLayout);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_places, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        navigateTo(item->data(Qt::UserRole).toString());
        m_places->clearSelection();
    });
    connect(m_backBtn,    &QToolButton::clicked, this, &QxFileDialog::goBack);
    connect(m_forwardBtn, &QToolButton::clicked, this, &QxFileDialog::goForward);
    connect(m_upBtn,      &QToolButton::clicked, this, &QxFileDialog::goUp);
    connect(m_breadcrumb, &QxFileBreadcrumb::pathActivated,
            this, [this](const QString& path) { navigateTo(path); });
    // Text typed, pasted or picked from history is resolved as a path on accept;
    // a name filled in from the view selection is accepted as is
    connect(m_fileEdit->lineEdit(), &QLineEdit::textEdited,
            this, [this] { m_nameFromSelection = false; });
    connect(m_fileEdit, QOverload<int>::of(&QComboBox::activated),
            this, [this] { m_nameFromSelection = false; });
    connect(m_view, &QTreeView::activated, this, &QxFileDialog::onItemActivated);
    connect(m_view, &QTreeView::customContextMenuRequested,
            this, &QxFileDialog::onViewContextMenu);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                onCurrentItemChanged(current);
            });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QxFileDialog::onFilterChanged);
    // Enter in the file edit reaches tryAccept() through the default button only
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]{ tryAccept(); });
    connect(cancelBtn,   &QPushButton::clicked, this, &QDialog::reject);

    setNameFilter("All Files (*)");
    navigateTo(QDir::currentPath());
}

void QxFileDialog::setDirectory(const QString& path)
{
    navigateTo(path);
}

void QxFileDialog::setNameFilter(const QString& filter)
{
    m_filters = parseFilter(filter);
    m_filterCombo->blockSignals(true);
    m_filterCombo->clear();
    for (const auto& f : m_filters) {
        QString label = f.display.isEmpty()
            ? f.patterns.join(" ")
            : f.display + " (" + f.patterns.join(" ") + ")";
        m_filterCombo->addItem(label);
    }
    m_filterCombo->blockSignals(false);
    applyCurrentFilter();
}

void QxFileDialog::setSizeUnit(SizeUnit unit)
{
    static_cast<DateFileSystemModel*>(m_model)->setSizeBase(static_cast<int>(unit));
    m_view->viewport()->update();
}

void QxFileDialog::setFileName(const QString& name)
{
    m_fileEdit->setCurrentText(name);
}

void QxFileDialog::setHistory(const QStringList& paths)
{
    // Bottom combo: full paths
    m_fileEdit->blockSignals(true);
    m_fileEdit->clear();
    m_fileEdit->addItems(paths);
    m_fileEdit->clearEditText();
    m_fileEdit->blockSignals(false);
}

void QxFileDialog::setDefaultSuffix(const QString& suffix)
{
    m_defaultSuffix = suffix;
}

QString QxFileDialog::selectedFile() const
{
    QString name = m_fileEdit->currentText().trimmed();
    if (name.isEmpty())
        return m_mode == Directory ? m_currentPath : QString{};
    if (QFileInfo(name).isAbsolute()) return name;
    return QDir(m_currentPath).filePath(name);
}

// ---------------------------------------------------------------------------

void QxFileDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_mode == Save && !m_fileEdit->currentText().isEmpty()) {
        m_fileEdit->setFocus();
        m_fileEdit->lineEdit()->selectAll();
    }
}

void QxFileDialog::navigateTo(const QString& path, bool pushToHistory)
{
    QDir dir(path);
    if (!dir.exists()) return;
    const QString canonical = dir.canonicalPath();

    if (pushToHistory) {
        while (m_history.size() > m_historyPos + 1)
            m_history.removeLast();
        m_history.append(canonical);
        m_historyPos = m_history.size() - 1;
    }

    m_currentPath = canonical;
    if (m_breadcrumb->path() != canonical)
        m_breadcrumb->setPath(canonical);
    m_view->setRootIndex(m_model->index(canonical));
    m_view->clearSelection();
    if (m_mode != Save) m_fileEdit->clearEditText();
    updateNavButtons();
}

void QxFileDialog::goBack()
{
    if (m_historyPos > 0) {
        --m_historyPos;
        navigateTo(m_history[m_historyPos], false);
    }
}

void QxFileDialog::goForward()
{
    if (m_historyPos < m_history.size() - 1) {
        ++m_historyPos;
        navigateTo(m_history[m_historyPos], false);
    }
}

void QxFileDialog::goUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp())
        navigateTo(dir.canonicalPath());
}

void QxFileDialog::updateNavButtons()
{
    m_backBtn->setEnabled(m_historyPos > 0);
    m_forwardBtn->setEnabled(m_historyPos < m_history.size() - 1);
    m_upBtn->setEnabled(!QDir(m_currentPath).isRoot());
}

void QxFileDialog::applyCurrentFilter()
{
    int idx = m_filterCombo->currentIndex();
    if (idx < 0 || idx >= m_filters.size()) return;
    m_model->setNameFilters(m_filters[idx].patterns);
    m_model->setNameFilterDisables(false);
}

// ---------------------------------------------------------------------------

void QxFileDialog::onItemActivated(const QModelIndex& index)
{
    if (m_model->isDir(index)) {
        navigateTo(m_model->filePath(index));
    } else {
        m_fileEdit->setCurrentText(m_model->fileName(index));
        tryAccept();
    }
}

void QxFileDialog::onCurrentItemChanged(const QModelIndex& current)
{
    if (!current.isValid()) return;
    if (m_mode == Directory) {
        m_fileEdit->setCurrentText(m_model->fileName(current));
    } else if (m_model->isDir(current)) {
        m_fileEdit->clearEditText();
    } else {
        m_fileEdit->setCurrentText(m_model->fileName(current));
    }
    m_nameFromSelection = true;
}

void QxFileDialog::onViewContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    const QModelIndex index = m_view->indexAt(pos);
    if (index.isValid()) {
        const QModelIndex nameIndex = index.siblingAtColumn(0);
        menu.addAction("Rename", this, [this, nameIndex] {
            m_view->setCurrentIndex(nameIndex);
            m_view->edit(nameIndex);
        });
        menu.addSeparator();
    }
    QMenu* newMenu = menu.addMenu("New");
    newMenu->addAction("Folder", this, &QxFileDialog::createFolder);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void QxFileDialog::createFolder()
{
    const QDir dir(m_currentPath);
    const QString base = "new_folder";
    QString name = base;
    for (int n = 1; QFileInfo::exists(dir.filePath(name)); ++n)
        name = QString("%1(%2)").arg(base).arg(n);

    const QModelIndex index = m_model->mkdir(m_model->index(m_currentPath), name);
    if (!index.isValid()) {
        QMessageBox::warning(this, "New Folder",
            QString("Cannot create folder \"%1\" in \"%2\".").arg(name, m_currentPath));
        return;
    }
    // Let the user choose the real name right away
    m_view->setCurrentIndex(index);
    m_view->scrollTo(index);
    m_view->edit(index);
}

void QxFileDialog::onFilterChanged(int /*index*/)
{
    applyCurrentFilter();
}

bool QxFileDialog::consumeTypedPath()
{
    const QString text = m_fileEdit->currentText().trimmed();
#ifdef Q_OS_WIN
    auto isSep = [](QChar c) { return c == '/' || c == '\\'; };
    const bool hasDrive = text.size() >= 2 && text[1] == ':' && text[0].isLetter();
#else
    auto isSep = [](QChar c) { return c == '/'; };
    const bool hasDrive = false;
#endif
    if (text.isEmpty())
        return false;

    // Starting point: drive root, file-system root, or the current directory
    QDir dir(m_currentPath);
    int pos = 0;
    if (hasDrive) {
        dir.setPath(text.left(2) + '/');
        pos = 2;
    } else if (isSep(text[0])) {
#ifdef Q_OS_WIN
        dir.setPath(QStorageInfo(m_currentPath).rootPath());  // root of the current drive
#else
        dir.setPath(QDir::rootPath());
#endif
    }

    // Enter each existing directory component; stop at the last component
    // (the file name) or at the first one that cannot be entered
    for (;;) {
        while (pos < text.size() && isSep(text[pos])) ++pos;
        int end = pos;
        while (end < text.size() && !isSep(text[end])) ++end;
        if (end == text.size()) break;
        const QString part = text.mid(pos, end - pos);
        if (part == "..")
            dir.cdUp();  // stays put at the root
        else if (part != "." && !dir.cd(part))
            break;
        pos = end;
    }

    // Last component: a directory is entered too; anything else is left
    // as the file name for the normal accept logic
    QString rest = text.mid(pos);
    const bool enterLast = rest.isEmpty() || rest == "." || rest == ".."
                        || QFileInfo(dir.filePath(rest)).isDir();
    const bool stoppedEarly = std::any_of(rest.begin(), rest.end(), isSep);
    if (enterLast && !stoppedEarly) {
        if (rest == "..") dir.cdUp();
        else if (!rest.isEmpty() && rest != ".") dir.cd(rest);
        rest.clear();
    }

    const QString target = dir.canonicalPath();
    if (target != m_currentPath)
        navigateTo(target);
    m_fileEdit->setCurrentText(rest);
    m_nameFromSelection = false;
    return enterLast || stoppedEarly;
}

bool QxFileDialog::tryAccept()
{
    // Directories (and unresolvable paths) only navigate; a file accepts at once
    if (!m_nameFromSelection && consumeTypedPath()) return false;

    QString file = selectedFile();
    if (file.isEmpty()) return false;

    if (m_mode == Directory) {
        if (!QDir(file).exists()) return false;
        accept();
        return true;
    }

    // Apply default suffix in Save mode when no extension given
    if (m_mode == Save && !m_defaultSuffix.isEmpty()) {
        QFileInfo info(file);
        if (info.suffix().isEmpty())
            file += "." + m_defaultSuffix;
        m_fileEdit->setCurrentText(QFileInfo(file).fileName());
    }

    if (m_mode == Open && !QFileInfo::exists(file)) return false;

    accept();
    return true;
}

// ---------------------------------------------------------------------------

QList<QxFileDialog::FilterEntry> QxFileDialog::parseFilter(const QString& filter)
{
    QList<FilterEntry> result;
    const QStringList parts = filter.split(";;", Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        FilterEntry e;
        const int parenOpen  = part.lastIndexOf('(');
        const int parenClose = part.lastIndexOf(')');
        if (parenOpen != -1 && parenClose > parenOpen) {
            e.display  = part.left(parenOpen).trimmed();
            const QString pat = part.mid(parenOpen + 1, parenClose - parenOpen - 1);
            e.patterns = pat.split(' ', Qt::SkipEmptyParts);
        } else {
            e.display  = part.trimmed();
            e.patterns = QStringList{"*"};
        }
        result.append(e);
    }
    if (result.isEmpty())
        result.append(FilterEntry{"All Files", QStringList{"*"}});
    return result;
}

// ---------------------------------------------------------------------------

QString QxFileDialog::getOpenFileName(QWidget* parent, const QString& caption,
                                       const QString& dir, const QString& filter,
                                       const QStringList& history)
{
    QxFileDialog dlg(parent, Open);
    if (!caption.isEmpty()) dlg.setWindowTitle(caption);
    if (!dir.isEmpty())     dlg.setDirectory(dir);
    if (!filter.isEmpty())  dlg.setNameFilter(filter);
    if (!history.isEmpty()) dlg.setHistory(history);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}

QString QxFileDialog::getSaveFileName(QWidget* parent, const QString& caption,
                                       const QString& dir, const QString& filter,
                                       const QString& defaultName,
                                       const QStringList& history)
{
    QxFileDialog dlg(parent, Save);
    if (!caption.isEmpty())     dlg.setWindowTitle(caption);
    if (!dir.isEmpty())         dlg.setDirectory(dir);
    if (!filter.isEmpty())      dlg.setNameFilter(filter);
    if (!defaultName.isEmpty()) dlg.setFileName(defaultName);
    if (!history.isEmpty())     dlg.setHistory(history);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}

QString QxFileDialog::getExistingDirectory(QWidget* parent, const QString& caption,
                                             const QString& dir,
                                             const QStringList& history)
{
    QxFileDialog dlg(parent, Directory);
    if (!caption.isEmpty())     dlg.setWindowTitle(caption);
    if (!dir.isEmpty())         dlg.setDirectory(dir);
    if (!history.isEmpty())     dlg.setHistory(history);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}
