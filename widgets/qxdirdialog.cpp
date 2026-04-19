#include "qxdirdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QDir>

QxDirDialog::QxDirDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("Select Directory"));
    resize(560, 360);
}

void QxDirDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("Recent Directories:"), this));

    m_recentListWidget = new QListWidget(this);
    m_recentListWidget->setAlternatingRowColors(true);
    connect(m_recentListWidget, &QListWidget::itemClicked,
            this, &QxDirDialog::onRecentItemClicked);
    connect(m_recentListWidget, &QListWidget::itemDoubleClicked,
            this, &QxDirDialog::onRecentItemDoubleClicked);
    mainLayout->addWidget(m_recentListWidget, 1);

    auto* dirRow = new QHBoxLayout;
    auto* dirLabel = new QLabel(tr("Dir:"), this);
    dirLabel->setFixedWidth(40);
    m_dirEdit = new QLineEdit(this);
    auto* browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &QxDirDialog::onBrowse);
    dirRow->addWidget(dirLabel);
    dirRow->addWidget(m_dirEdit, 1);
    dirRow->addWidget(browseBtn);
    mainLayout->addLayout(dirRow);

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QxDirDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QxDirDialog::setRecentDirs(const QStringList& dirs)
{
    m_recentDirs = dirs;
    updateRecentList();
}

void QxDirDialog::updateRecentList()
{
    m_recentListWidget->clear();
    for (const QString& d : m_recentDirs)
        m_recentListWidget->addItem(d);
}

void QxDirDialog::addToRecent(const QString& dirPath)
{
    m_recentDirs.removeAll(dirPath);
    m_recentDirs.prepend(dirPath);
    while (m_recentDirs.size() > MAX_RECENT)
        m_recentDirs.removeLast();
    updateRecentList();
}

void QxDirDialog::onBrowse()
{
    QString selected = QFileDialog::getExistingDirectory(
        this, windowTitle(), m_directory.isEmpty() ? QDir::homePath() : m_directory);
    if (!selected.isEmpty()) {
        m_dirEdit->setText(selected);
        addToRecent(selected);
    }
}

void QxDirDialog::onRecentItemClicked(QListWidgetItem* item)
{
    if (item)
        m_dirEdit->setText(item->text());
}

void QxDirDialog::onRecentItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    m_selectedDir = item->text();
    addToRecent(m_selectedDir);
    QDialog::accept();
}

void QxDirDialog::onAccept()
{
    QString path = m_dirEdit->text().trimmed();
    if (path.isEmpty()) return;
    if (!QDir(path).exists()) return;

    m_selectedDir = path;
    addToRecent(path);
    QDialog::accept();
}

QString QxDirDialog::getExistingDirectory(QWidget* parent, const QString& title,
                                           const QString& dir, QStringList* recentDirs)
{
    QxDirDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    if (recentDirs)
        dlg.setRecentDirs(*recentDirs);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentDirs)
            *recentDirs = dlg.recentDirs();
        return dlg.selectedDir();
    }
    return {};
}