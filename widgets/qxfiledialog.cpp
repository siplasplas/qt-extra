#include "qxfiledialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDialogButtonBox>

QxFileDialog::QxFileDialog(Mode mode, QWidget* parent)
    : QDialog(parent), m_mode(mode)
{
    setupUi();
    setWindowTitle(mode == Open ? tr("Open File") : tr("Save File"));
    resize(560, 380);
}

void QxFileDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(new QLabel(tr("Recent Files:"), this));

    m_recentListWidget = new QListWidget(this);
    m_recentListWidget->setAlternatingRowColors(true);
    connect(m_recentListWidget, &QListWidget::itemClicked,
            this, &QxFileDialog::onRecentItemClicked);
    connect(m_recentListWidget, &QListWidget::itemDoubleClicked,
            this, &QxFileDialog::onRecentItemDoubleClicked);
    mainLayout->addWidget(m_recentListWidget, 1);

    auto* fileRow = new QHBoxLayout;
    auto* fileLabel = new QLabel(tr("File:"), this);
    fileLabel->setFixedWidth(40);
    m_fileEdit = new QLineEdit(this);
    auto* browseBtn = new QPushButton(tr("Browse..."), this);
    connect(browseBtn, &QPushButton::clicked, this, &QxFileDialog::onBrowse);
    fileRow->addWidget(fileLabel);
    fileRow->addWidget(m_fileEdit, 1);
    fileRow->addWidget(browseBtn);
    mainLayout->addLayout(fileRow);

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QxFileDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QxFileDialog::setRecentFiles(const QStringList& files)
{
    m_recentFiles = files;
    updateRecentList();
}

void QxFileDialog::updateRecentList()
{
    m_recentListWidget->clear();
    for (const QString& f : m_recentFiles)
        m_recentListWidget->addItem(f);
}

void QxFileDialog::addToRecent(const QString& filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > MAX_RECENT)
        m_recentFiles.removeLast();
    updateRecentList();
}

void QxFileDialog::onBrowse()
{
    QString selected;
    if (m_mode == Open)
        selected = QFileDialog::getOpenFileName(this, windowTitle(), m_directory, m_nameFilter);
    else
        selected = QFileDialog::getSaveFileName(this, windowTitle(), m_directory, m_nameFilter);

    if (!selected.isEmpty()) {
        m_fileEdit->setText(selected);
        addToRecent(selected);
    }
}

void QxFileDialog::onRecentItemClicked(QListWidgetItem* item)
{
    if (item)
        m_fileEdit->setText(item->text());
}

void QxFileDialog::onRecentItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    m_selectedFile = item->text();
    addToRecent(m_selectedFile);
    QDialog::accept();
}

void QxFileDialog::onAccept()
{
    QString path = m_fileEdit->text().trimmed();
    if (path.isEmpty()) return;

    if (m_mode == Open && !QFileInfo::exists(path)) return;

    if (m_mode == Save && !m_defaultSuffix.isEmpty()) {
        QFileInfo fi(path);
        if (fi.suffix().isEmpty())
            path += QLatin1Char('.') + m_defaultSuffix;
    }

    m_selectedFile = path;
    addToRecent(path);
    QDialog::accept();
}

QString QxFileDialog::getOpenFileName(QWidget* parent, const QString& title,
                                      const QString& dir, const QString& nameFilter,
                                      QStringList* recentFiles)
{
    QxFileDialog dlg(Open, parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    dlg.setNameFilter(nameFilter);
    if (recentFiles)
        dlg.setRecentFiles(*recentFiles);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentFiles)
            *recentFiles = dlg.recentFiles();
        return dlg.selectedFile();
    }
    return {};
}

QString QxFileDialog::getSaveFileName(QWidget* parent, const QString& title,
                                      const QString& dir, const QString& nameFilter,
                                      QStringList* recentFiles)
{
    QxFileDialog dlg(Save, parent);
    dlg.setWindowTitle(title);
    dlg.setInitialDirectory(dir);
    dlg.setNameFilter(nameFilter);
    if (recentFiles)
        dlg.setRecentFiles(*recentFiles);

    if (dlg.exec() == QDialog::Accepted) {
        if (recentFiles)
            *recentFiles = dlg.recentFiles();
        return dlg.selectedFile();
    }
    return {};
}