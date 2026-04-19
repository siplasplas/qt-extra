#include "qxfilebrowser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeView>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QFileSystemModel>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QKeyEvent>

// QFileSystemModel subclass — overrides date column to use yyyy-MM-dd HH:mm format
class DateFileSystemModel : public QFileSystemModel {
public:
    using QFileSystemModel::QFileSystemModel;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && index.column() == 3)
            return fileInfo(index).lastModified().toString("yyyy-MM-dd HH:mm");
        return QFileSystemModel::data(index, role);
    }
};

QxFileBrowser::QxFileBrowser(QWidget* parent, Mode mode)
    : QDialog(parent), m_mode(mode)
{
    setWindowTitle(mode == Open ? "Open File" : "Save File");
    resize(720, 520);

    // Model — watch full filesystem; view root index will select the directory
    m_model = new DateFileSystemModel(this);
    m_model->setRootPath(QDir::rootPath());
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    // File tree view (multi-column)
    m_view = new QTreeView;
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setColumnHidden(2, true);          // hide "Type" column
    m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_view->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_view->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Navigation bar
    m_backBtn    = new QToolButton; m_backBtn->setText("←");
    m_forwardBtn = new QToolButton; m_forwardBtn->setText("→");
    m_upBtn      = new QToolButton; m_upBtn->setText("↑");
    m_pathEdit   = new QLineEdit;
    m_pathEdit->setPlaceholderText("Path — type or paste and press Enter");

    auto* navLayout = new QHBoxLayout;
    navLayout->addWidget(m_backBtn);
    navLayout->addWidget(m_forwardBtn);
    navLayout->addWidget(m_upBtn);
    navLayout->addWidget(m_pathEdit, 1);

    // Bottom bar
    m_fileEdit    = new QLineEdit;
    m_filterCombo = new QComboBox;
    m_filterCombo->setMinimumWidth(200);
    m_acceptBtn   = new QPushButton(mode == Open ? "Open" : "Save");
    auto* cancelBtn = new QPushButton("Cancel");
    m_acceptBtn->setDefault(true);

    auto* bottomGrid = new QGridLayout;
    bottomGrid->addWidget(new QLabel("File name:"), 0, 0);
    bottomGrid->addWidget(m_fileEdit,               0, 1);
    bottomGrid->addWidget(new QLabel("File type:"), 1, 0);
    bottomGrid->addWidget(m_filterCombo,            1, 1);
    bottomGrid->setColumnStretch(1, 1);

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_acceptBtn);
    btnLayout->addWidget(cancelBtn);

    // Main layout
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(navLayout);
    mainLayout->addWidget(m_view, 1);
    mainLayout->addLayout(bottomGrid);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_backBtn,    &QToolButton::clicked, this, &QxFileBrowser::goBack);
    connect(m_forwardBtn, &QToolButton::clicked, this, &QxFileBrowser::goForward);
    connect(m_upBtn,      &QToolButton::clicked, this, &QxFileBrowser::goUp);
    m_pathEdit->installEventFilter(this);
    connect(m_fileEdit,   &QLineEdit::returnPressed, this, &QxFileBrowser::onFileEditReturnPressed);
    connect(m_view, &QTreeView::activated, this, &QxFileBrowser::onItemActivated);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
                onCurrentItemChanged(current);
            });
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QxFileBrowser::onFilterChanged);
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]{ tryAccept(); });
    connect(cancelBtn,   &QPushButton::clicked, this, &QDialog::reject);

    setNameFilter("All Files (*)");
}

void QxFileBrowser::setDirectory(const QString& path)
{
    navigateTo(path);
}

void QxFileBrowser::setNameFilter(const QString& filter)
{
    m_filters = parseFilter(filter);
    m_filterCombo->blockSignals(true);
    m_filterCombo->clear();
    for (const auto& f : m_filters)
        m_filterCombo->addItem(f.display.isEmpty() ? f.patterns.join(" ") : f.display);
    m_filterCombo->blockSignals(false);
    applyCurrentFilter();
}

void QxFileBrowser::setDefaultSuffix(const QString& suffix)
{
    m_defaultSuffix = suffix;
}

QString QxFileBrowser::selectedFile() const
{
    QString name = m_fileEdit->text().trimmed();
    if (name.isEmpty()) return {};
    if (QFileInfo(name).isAbsolute()) return name;
    return QDir(m_currentPath).filePath(name);
}

// ---------------------------------------------------------------------------

void QxFileBrowser::navigateTo(const QString& path, bool pushToHistory)
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
    m_pathEdit->setText(canonical);
    m_view->setRootIndex(m_model->index(canonical));
    m_view->clearSelection();
    updateNavButtons();
}

void QxFileBrowser::goBack()
{
    if (m_historyPos > 0) {
        --m_historyPos;
        navigateTo(m_history[m_historyPos], false);
    }
}

void QxFileBrowser::goForward()
{
    if (m_historyPos < m_history.size() - 1) {
        ++m_historyPos;
        navigateTo(m_history[m_historyPos], false);
    }
}

void QxFileBrowser::goUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp())
        navigateTo(dir.canonicalPath());
}

void QxFileBrowser::updateNavButtons()
{
    m_backBtn->setEnabled(m_historyPos > 0);
    m_forwardBtn->setEnabled(m_historyPos < m_history.size() - 1);
    m_upBtn->setEnabled(!QDir(m_currentPath).isRoot());
}

void QxFileBrowser::applyCurrentFilter()
{
    int idx = m_filterCombo->currentIndex();
    if (idx < 0 || idx >= m_filters.size()) return;
    m_model->setNameFilters(m_filters[idx].patterns);
    m_model->setNameFilterDisables(false);
}

// ---------------------------------------------------------------------------

void QxFileBrowser::onItemActivated(const QModelIndex& index)
{
    if (m_model->isDir(index)) {
        navigateTo(m_model->filePath(index));
    } else {
        m_fileEdit->setText(m_model->fileName(index));
        tryAccept();
    }
}

void QxFileBrowser::onCurrentItemChanged(const QModelIndex& current)
{
    if (!current.isValid()) return;
    if (m_model->isDir(current))
        m_fileEdit->clear();
    else
        m_fileEdit->setText(m_model->fileName(current));
}

bool QxFileBrowser::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_pathEdit && event->type() == QEvent::KeyPress) {
        const auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            onPathEditReturnPressed();
            return true;  // consume — prevents QDialog default button from also firing
        }
    }
    return QDialog::eventFilter(obj, event);
}

void QxFileBrowser::onPathEditReturnPressed()
{
    const QString text = m_pathEdit->text().trimmed();
    QFileInfo info(text);
    if (info.isDir()) {
        navigateTo(text);
    } else {
        // Not a directory — navigate to parent if it exists, put filename in file edit
        QDir parent = info.dir();
        if (parent.exists()) {
            navigateTo(parent.canonicalPath());
            m_fileEdit->setText(info.fileName());
        }
    }
}

void QxFileBrowser::onFileEditReturnPressed()
{
    tryAccept();
}

void QxFileBrowser::onFilterChanged(int /*index*/)
{
    applyCurrentFilter();
}

bool QxFileBrowser::tryAccept()
{
    QString file = selectedFile();
    if (file.isEmpty()) return false;

    // Apply default suffix in Save mode when no extension given
    if (m_mode == Save && !m_defaultSuffix.isEmpty()) {
        QFileInfo info(file);
        if (info.suffix().isEmpty())
            file += "." + m_defaultSuffix;
        m_fileEdit->setText(QFileInfo(file).fileName());
    }

    if (m_mode == Open && !QFileInfo::exists(file)) return false;

    accept();
    return true;
}

// ---------------------------------------------------------------------------

QList<QxFileBrowser::FilterEntry> QxFileBrowser::parseFilter(const QString& filter)
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

QString QxFileBrowser::getOpenFileName(QWidget* parent, const QString& caption,
                                       const QString& dir, const QString& filter)
{
    QxFileBrowser dlg(parent, Open);
    if (!caption.isEmpty()) dlg.setWindowTitle(caption);
    if (!dir.isEmpty())     dlg.setDirectory(dir);
    if (!filter.isEmpty())  dlg.setNameFilter(filter);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}

QString QxFileBrowser::getSaveFileName(QWidget* parent, const QString& caption,
                                       const QString& dir, const QString& filter)
{
    QxFileBrowser dlg(parent, Save);
    if (!caption.isEmpty()) dlg.setWindowTitle(caption);
    if (!dir.isEmpty())     dlg.setDirectory(dir);
    if (!filter.isEmpty())  dlg.setNameFilter(filter);
    return dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
}