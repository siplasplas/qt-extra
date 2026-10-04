#include "qxrecentdialog.h"
#include "qxfiledialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

QxRecentDialog::QxRecentDialog(Mode mode, QWidget* parent)
    : QDialog(parent), m_mode(mode)
{
    setupUi();
    setWindowTitle(mode == Open ? tr("Open File")
                                : mode == Save ? tr("Save File") : tr("Select Directory"));
    resize(560, mode == Directory ? 360 : 380);
}

void QxRecentDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(
        m_mode == Directory ? tr("Recent Directories:") : tr("Recent Files:"), this));

    m_recentListWidget = new QListWidget(this);
    m_recentListWidget->setAlternatingRowColors(true);
    connect(m_recentListWidget, &QListWidget::itemClicked,
            this, &QxRecentDialog::onRecentItemClicked);
    connect(m_recentListWidget, &QListWidget::itemDoubleClicked,
            this, &QxRecentDialog::onRecentItemDoubleClicked);
    mainLayout->addWidget(m_recentListWidget, 1);

    auto* pathRow = new QHBoxLayout;
    auto* pathLabel = new QLabel(m_mode == Directory ? tr("Dir:") : tr("File:"), this);
    pathLabel->setFixedWidth(40);
    m_pathEdit = new QLineEdit(this);
    auto* browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &QxRecentDialog::onBrowse);
    pathRow->addWidget(pathLabel);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseBtn);
    mainLayout->addLayout(pathRow);

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QxRecentDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QxRecentDialog::setRecentPaths(const QStringList& paths)
{
    m_recentPaths = paths;
    updateRecentList();
}

void QxRecentDialog::updateRecentList()
{
    m_recentListWidget->clear();
    for (const QString& path : m_recentPaths)
        m_recentListWidget->addItem(path);
}

void QxRecentDialog::addToRecent(const QString& path)
{
    m_recentPaths.removeAll(path);
    m_recentPaths.prepend(path);
    while (m_recentPaths.size() > MAX_RECENT)
        m_recentPaths.removeLast();
    updateRecentList();
}

void QxRecentDialog::onBrowse()
{
    QString selected;
    if (m_mode == Open)
        selected = QxFileDialog::getOpenFileName(this, windowTitle(), m_directory, m_nameFilter);
    else if (m_mode == Save)
        selected = QxFileDialog::getSaveFileName(this, windowTitle(), m_directory, m_nameFilter);
    else
        selected = QxFileDialog::getExistingDirectory(
            this, windowTitle(), m_directory.isEmpty() ? QDir::homePath() : m_directory);

    if (!selected.isEmpty()) {
        m_pathEdit->setText(selected);
        addToRecent(selected);
    }
}

void QxRecentDialog::onRecentItemClicked(QListWidgetItem* item)
{
    if (item)
        m_pathEdit->setText(item->text());
}

void QxRecentDialog::onRecentItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    m_selectedPath = item->text();
    addToRecent(m_selectedPath);
    QDialog::accept();
}

void QxRecentDialog::onAccept()
{
    QString path = QxFileDialog::expandHomePath(m_pathEdit->text().trimmed());
    if (path.isEmpty()) return;
    if (m_mode == Open && !QFileInfo::exists(path)) return;
    if (m_mode == Directory && !QDir(path).exists()) return;

    if (m_mode == Save && !m_defaultSuffix.isEmpty()) {
        QFileInfo fileInfo(path);
        if (fileInfo.suffix().isEmpty())
            path += QLatin1Char('.') + m_defaultSuffix;
    }

    m_selectedPath = path;
    addToRecent(path);
    QDialog::accept();
}

QString QxRecentDialog::getOpenFileName(QWidget* parent, const QString& title,
                                         const QString& dir, const QString& nameFilter,
                                         QStringList* recentPaths)
{
    QxRecentDialog dlg(Open, parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    dlg.setNameFilter(nameFilter);
    if (recentPaths)
        dlg.setRecentPaths(*recentPaths);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentPaths)
            *recentPaths = dlg.recentPaths();
        return dlg.selectedPath();
    }
    return {};
}

QString QxRecentDialog::getSaveFileName(QWidget* parent, const QString& title,
                                         const QString& dir, const QString& nameFilter,
                                         QStringList* recentPaths)
{
    QxRecentDialog dlg(Save, parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    dlg.setNameFilter(nameFilter);
    if (recentPaths)
        dlg.setRecentPaths(*recentPaths);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentPaths)
            *recentPaths = dlg.recentPaths();
        return dlg.selectedPath();
    }
    return {};
}

QString QxRecentDialog::getExistingDirectory(QWidget* parent, const QString& title,
                                               const QString& dir, QStringList* recentPaths)
{
    QxRecentDialog dlg(Directory, parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    if (recentPaths)
        dlg.setRecentPaths(*recentPaths);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentPaths)
            *recentPaths = dlg.recentPaths();
        return dlg.selectedPath();
    }
    return {};
}
