#include "qxfiledialog.h"
#include "qxfilebreadcrumb.h"
#include "qxfilemetadata.h"

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
#include <QTimer>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QCoreApplication>

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
    m_metadata = new QxMetadataModel(this);
    m_metadata->setSourceModel(m_model);
    m_proxy = new QxMetadataSortModel(this);
    m_proxy->setSourceModel(m_metadata);
    connect(m_model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& first, const QModelIndex& last) {
                const QModelIndex a = m_metadata->mapFromSource(first).siblingAtColumn(4);
                const QModelIndex b = m_metadata->mapFromSource(last).siblingAtColumn(6);
                emit m_metadata->dataChanged(a, b);
            });

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
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setColumnHidden(1, mode == Directory); // hide Size in dir mode
    m_view->setColumnHidden(2, true);              // hide "Type" column
    for (int column = 4; column <= 6; ++column) {
        m_view->setColumnHidden(column, true);
    }
    auto* header = m_view->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->resizeSection(0, 280);
    const QString samples[] = {"999.9 MiB", "", "yyyy-MM-dd HH:mm", "999:59.9", "99999", "99999"};
    for (int column = 1; column <= 6; ++column) {
        const QString title = m_proxy->headerData(column, Qt::Horizontal).toString();
        header->resizeSection(column, qMax(header->fontMetrics().horizontalAdvance(title),
                                          header->fontMetrics().horizontalAdvance(samples[column - 1])) + 32);
    }
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
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
    auto typedName = [this] {
        m_pendingFile.clear();
        m_nameFromSelection = false;
        // A typed path supersedes any multiple selection without changing the text.
        const QSignalBlocker blocker(m_view->selectionModel());
        m_view->clearSelection();
    };
    // Typing a name (no directory separator) quick-searches the list: the selection
    // moves to the nearest name containing the text, or stays put without a match
    connect(m_fileEdit->lineEdit(), &QLineEdit::textEdited, this, [this, typedName] {
        if (quickSearchText().isEmpty()) {
            typedName();
            return;
        }
        m_pendingFile.clear();
        m_nameFromSelection = false;
        moveToQuickMatch(0);
    });
    connect(m_fileEdit, QOverload<int>::of(&QComboBox::activated), this, typedName);
    connect(m_view, &QTreeView::activated, this, &QxFileDialog::onItemActivated);
    connect(m_view, &QTreeView::customContextMenuRequested,
            this, &QxFileDialog::onViewContextMenu);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                onCurrentItemChanged(current);
            });
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this] {
                if (m_multipleSelection && !m_quickSearching) updateSelectionName();
            });
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, [this] { scheduleFileSelection(); });
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, [this] { scheduleFileSelection(); });
    connect(m_model, &QFileSystemModel::directoryLoaded, this, [this](const QString& path) {
        m_loadedDirectories.insert(QDir::cleanPath(path));
        scheduleFileSelection();
    });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QxFileDialog::onFilterChanged);
    // Enter in the file edit reaches tryAccept() through the default button only
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]{ tryAccept(); });
    connect(cancelBtn,   &QPushButton::clicked, this, &QDialog::reject);
    m_view->installEventFilter(this);
    m_fileEdit->installEventFilter(this);              // has the focus; forwards text keys
    m_fileEdit->lineEdit()->installEventFilter(this);

    setNameFilter("All Files (*)");
    navigateTo(QDir::currentPath());
}

void QxFileDialog::setDirectory(const QString& path)
{
    navigateTo(path);
}

QString QxFileDialog::directory() const
{
    return m_currentPath;
}

void QxFileDialog::setAudioDurationVisible(bool visible)
{
    m_metadata->setFeatures(visible, m_metadata->imagesEnabled());
    if (!visible && m_view->header()->sortIndicatorSection() == 4)
        m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setColumnHidden(4, !visible);
}

void QxFileDialog::setImageDimensionsVisible(bool visible)
{
    m_metadata->setFeatures(m_metadata->audioEnabled(), visible);
    if (!visible && m_view->header()->sortIndicatorSection() >= 5)
        m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setColumnHidden(5, !visible);
    m_view->setColumnHidden(6, !visible);
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
    m_pendingFile.clear();
    {
        const QSignalBlocker blocker(m_view->selectionModel());
        m_view->clearSelection();
    }
    m_fileEdit->setCurrentText(name);
    m_nameFromSelection = false;
    if (name.isEmpty()) return;
    const QFileInfo info(QDir(m_currentPath).filePath(name));
    if (m_mode == Directory) {
        if (info.isDir()) {
            navigateTo(info.absoluteFilePath());
            m_fileEdit->clearEditText();
        }
        return;
    }
    if (QDir(info.absolutePath()).exists()) {
        if (QDir(info.absolutePath()).canonicalPath() != m_currentPath)
            navigateTo(info.absolutePath());
        m_fileEdit->setCurrentText(info.fileName());
        m_nameFromSelection = false;
        if (info.isFile()) {
            m_pendingFile = QDir(m_currentPath).filePath(info.fileName());
            scheduleFileSelection();
        }
    }
}

void QxFileDialog::setMultipleSelectionEnabled(bool enabled)
{
    enabled = enabled && m_mode == Open;
    if (m_multipleSelection == enabled) return;
    const QModelIndex current = m_view->currentIndex();
    m_multipleSelection = enabled;
    m_view->setSelectionMode(enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    if (!enabled && current.isValid()) {
        m_view->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        onCurrentItemChanged(current);
    } else if (enabled && m_nameFromSelection) {
        updateSelectionName();
    }
}

void QxFileDialog::scheduleFileSelection()
{
    if (m_pendingFile.isEmpty() || m_selectionScheduled) return;
    m_selectionScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_selectionScheduled = false;
        selectPendingFile();
    });
}

void QxFileDialog::selectPendingFile()
{
    if (m_pendingFile.isEmpty() || !isVisible() || !m_loadedDirectories.contains(m_currentPath)) return;
    const QModelIndex source = m_model->index(m_pendingFile);
    const QModelIndex index = m_proxy->mapFromSource(m_metadata->mapFromSource(source));
    if (!index.isValid() || index.parent() != m_view->rootIndex()) return;
    m_pendingFile.clear();
    m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_view->doItemsLayout();
    m_view->scrollTo(index, QAbstractItemView::EnsureVisible);
}

void QxFileDialog::updateSelectionName()
{
    auto rows = m_view->selectionModel()->selectedRows(0);
    std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
    QStringList names;
    for (const auto& row : rows) {
        const QModelIndex source = m_metadata->mapToSource(m_proxy->mapToSource(row));
        if (m_model->fileInfo(source).isFile()) names.append(m_model->fileName(source));
    }
    if (names.size() > 1) {
        for (QString& name : names) {
            name.replace("\\", "\\\\");
            name.replace("\"", "\\\"");
            name = '"' + name + '"';
        }
    }
    m_fileEdit->setCurrentText(names.join(' '));
    m_nameFromSelection = !rows.isEmpty();
    if (!rows.isEmpty()) m_pendingFile.clear();
}

void QxFileDialog::setHistory(const QStringList& paths)
{
    // Bottom combo: full paths
    m_fileEdit->blockSignals(true);
    m_fileEdit->clear();
    m_fileEdit->addItems(paths);
    m_fileEdit->clearEditText();
    m_fileEdit->blockSignals(false);
    m_pendingFile.clear();
    m_nameFromSelection = false;
    const QSignalBlocker blocker(m_view->selectionModel());
    m_view->clearSelection();
}

void QxFileDialog::setDefaultSuffix(const QString& suffix)
{
    m_defaultSuffix = suffix;
}

QString QxFileDialog::selectedFile() const
{
    if (m_multipleSelection && m_nameFromSelection) return selectedFiles().value(0);
    QString name = m_fileEdit->currentText().trimmed();
    if (name.isEmpty())
        return m_mode == Directory ? m_currentPath : QString{};
    if (QFileInfo(name).isAbsolute()) return name;
    return QDir(m_currentPath).filePath(name);
}

QStringList QxFileDialog::selectedFiles() const
{
    if (m_multipleSelection && m_nameFromSelection) {
        auto rows = m_view->selectionModel()->selectedRows(0);
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() < b.row(); });
        QStringList files;
        for (const auto& row : rows) {
            const QModelIndex source = m_metadata->mapToSource(m_proxy->mapToSource(row));
            if (m_model->fileInfo(source).isFile()) files.append(m_model->filePath(source));
        }
        return files;
    }
    const QString file = selectedFile();
    return file.isEmpty() ? QStringList{} : QStringList{file};
}

// ---------------------------------------------------------------------------

void QxFileDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    m_metadata->setActive(true);
    scheduleFileSelection();
    // Typing goes to the name field right away (and quick-searches the list)
    m_fileEdit->setFocus();
    if (m_mode == Save && !m_fileEdit->currentText().isEmpty())
        m_fileEdit->lineEdit()->selectAll();
}

void QxFileDialog::hideEvent(QHideEvent* event)
{
    m_metadata->setActive(false);
    m_pendingFile.clear();
    QDialog::hideEvent(event);
}

bool QxFileDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress)
        return QDialog::eventFilter(watched, event);
    auto* key = static_cast<QKeyEvent*>(event);
    QLineEdit* edit = m_fileEdit->lineEdit();

    if (watched == m_view) {
        // A letter or digit typed in the list starts a new name in the name field
        const QString text = key->text();
        const Qt::KeyboardModifiers mods = key->modifiers()
            & ~(Qt::ShiftModifier | Qt::KeypadModifier | Qt::GroupSwitchModifier);
        if (mods == Qt::NoModifier && text.size() == 1 && text[0].isLetterOrNumber()) {
            m_fileEdit->setFocus();
            edit->selectAll();
            QKeyEvent copy(key->type(), key->key(), key->modifiers(), text);
            QCoreApplication::sendEvent(m_fileEdit, &copy);
            return true;
        }
    } else if (watched == m_fileEdit || watched == edit) {
        const bool searching = !m_nameFromSelection && !quickSearchText().isEmpty();
        switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
            if (key->modifiers() & (Qt::AltModifier | Qt::ControlModifier))
                break;  // Alt+Down still opens the history
            if (searching) {
                const bool down = key->key() == Qt::Key_Down || key->key() == Qt::Key_PageDown;
                moveToQuickMatch(down ? 1 : -1);
            } else {
                // Without a search the keys move through the list, focus stays here
                QKeyEvent copy(key->type(), key->key(), key->modifiers(), key->text());
                QCoreApplication::sendEvent(m_view, &copy);
            }
            return true;
        case Qt::Key_Right:
            if (key->modifiers() == Qt::NoModifier && !edit->hasSelectedText()
                && edit->cursorPosition() == edit->text().size() && completeQuickSearch())
                return true;
            break;
        case Qt::Key_Tab:
            if (key->modifiers() == Qt::NoModifier && completeQuickSearch())
                return true;
            break;
        default:
            break;
        }
    }
    return QDialog::eventFilter(watched, event);
}

// Typed text usable as a quick search: a plain name, not a path
QString QxFileDialog::quickSearchText() const
{
    const QString text = m_fileEdit->currentText().trimmed();
#ifdef Q_OS_WIN
    if (text.contains('\\') || text.contains(':')) return {};
#endif
    return text.contains('/') ? QString{} : text;
}

// Case-insensitive key that ignores diacritics ("Łódź" matches "lodz")
static QString quickSearchKey(const QString& text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString key;
    key.reserve(decomposed.size());
    for (QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing) continue;
        if (c == QChar(0x0141) || c == QChar(0x0142)) c = QLatin1Char('l');  // Ł, ł
        key += c;
    }
    return key.toCaseFolded();
}

bool QxFileDialog::currentMatchesQuickSearch() const
{
    const QString needle = quickSearchKey(quickSearchText());
    const QModelIndex current = m_view->currentIndex();
    return !needle.isEmpty() && current.isValid() && current.parent() == m_view->rootIndex()
        && m_view->selectionModel()->isSelected(current)
        && quickSearchKey(current.siblingAtColumn(0).data().toString()).contains(needle);
}

// Selects the nearest item whose name contains the typed text: from the current
// item inclusive (step 0), or the next (1) / previous (-1) one, wrapping around.
// Without a match the selection stays where it is. The typed text is kept.
bool QxFileDialog::moveToQuickMatch(int step)
{
    const QString needle = quickSearchKey(quickSearchText());
    if (needle.isEmpty()) return false;
    const QModelIndex root = m_view->rootIndex();
    const int rows = m_proxy->rowCount(root);
    if (rows == 0) return false;

    const QModelIndex current = m_view->currentIndex();
    const bool hasCurrent = current.isValid() && current.parent() == root;
    int base = hasCurrent ? current.row() : 0;
    if (!hasCurrent && step != 0) base = step > 0 ? -1 : rows;
    for (int i = 0; i < rows; ++i) {
        const int offset = step == 0 ? i : (i + 1) * (step > 0 ? 1 : -1);
        const int row = ((base + offset) % rows + rows) % rows;
        const QModelIndex index = m_proxy->index(row, 0, root);
        if (!quickSearchKey(index.data().toString()).contains(needle)) continue;
        m_quickSearching = true;
        m_view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_quickSearching = false;
        m_view->scrollTo(index);
        return true;
    }
    return false;
}

// Right/Tab: completes the typed text to the name of the matched item
bool QxFileDialog::completeQuickSearch()
{
    if (m_nameFromSelection || !currentMatchesQuickSearch()) return false;
    const QString name = m_view->currentIndex().siblingAtColumn(0).data().toString();
    if (m_fileEdit->currentText() == name) return false;
    m_fileEdit->setCurrentText(name);
    m_nameFromSelection = false;  // resolved like a typed name: a directory is entered on accept
    return true;
}

void QxFileDialog::navigateTo(const QString& path, bool pushToHistory)
{
    QDir dir(path);
    if (!dir.exists()) return;
    const QString canonical = dir.canonicalPath();
    m_pendingFile.clear();
    m_nameFromSelection = false;

    if (pushToHistory) {
        while (m_history.size() > m_historyPos + 1)
            m_history.removeLast();
        m_history.append(canonical);
        m_historyPos = m_history.size() - 1;
    }

    m_currentPath = canonical;
    m_metadata->setDirectory(canonical);
    if (m_breadcrumb->path() != canonical)
        m_breadcrumb->setPath(canonical);
    m_view->setRootIndex(m_proxy->mapFromSource(m_metadata->mapFromSource(m_model->index(canonical))));
    m_view->clearSelection();
    if (m_mode != Save) m_fileEdit->clearEditText();
    m_nameFromSelection = false;
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
    const QModelIndex source = m_metadata->mapToSource(m_proxy->mapToSource(index));
    if (m_model->isDir(source)) {
        navigateTo(m_model->filePath(source));
    } else {
        if (m_multipleSelection && !m_view->selectionModel()->isSelected(index))
            m_view->selectionModel()->setCurrentIndex(index.siblingAtColumn(0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_fileEdit->setCurrentText(m_model->fileName(source));
        m_nameFromSelection = true;
        tryAccept();
    }
}

void QxFileDialog::onCurrentItemChanged(const QModelIndex& current)
{
    if (!current.isValid() || m_quickSearching) return;
    m_pendingFile.clear();
    if (m_multipleSelection) {
        updateSelectionName();
        return;
    }
    const QModelIndex source = m_metadata->mapToSource(m_proxy->mapToSource(current));
    if (m_mode == Directory) {
        m_fileEdit->setCurrentText(m_model->fileName(source));
    } else if (m_model->isDir(source)) {
        m_fileEdit->clearEditText();
    } else {
        m_fileEdit->setCurrentText(m_model->fileName(source));
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
    const QModelIndex viewIndex = m_proxy->mapFromSource(m_metadata->mapFromSource(index));
    m_view->setCurrentIndex(viewIndex);
    m_view->scrollTo(viewIndex);
    m_view->edit(viewIndex);
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
    if (m_multipleSelection && m_nameFromSelection) {
        const QStringList files = selectedFiles();
        if (files.isEmpty()) return false;
        for (const QString& file : files)
            if (!QFileInfo(file).isFile()) return false;
        accept();
        return true;
    }
    // A partial name that is not an entry itself opens the item quick search
    // matched (Save keeps the typed name: it may be a new file)
    if (!m_nameFromSelection && m_mode != Save && currentMatchesQuickSearch()
        && !QFileInfo::exists(QDir(m_currentPath).filePath(quickSearchText()))) {
        onItemActivated(m_view->currentIndex());
        return result() == QDialog::Accepted;
    }
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
    if (!history.isEmpty())     dlg.setHistory(history);
    if (!defaultName.isEmpty()) dlg.setFileName(defaultName);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}

QStringList QxFileDialog::getOpenFileNames(QWidget* parent, const QString& caption,
                                         const QString& dir, const QString& filter,
                                         const QStringList& history)
{
    QxFileDialog dlg(parent, Open);
    dlg.setMultipleSelectionEnabled(true);
    if (!caption.isEmpty()) dlg.setWindowTitle(caption);
    if (!dir.isEmpty())     dlg.setDirectory(dir);
    if (!filter.isEmpty())  dlg.setNameFilter(filter);
    if (!history.isEmpty()) dlg.setHistory(history);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFiles() : QStringList{};
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
