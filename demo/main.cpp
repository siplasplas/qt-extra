#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QDir>

#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxdirdialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("qt-extra demo");

    QMainWindow mainWindow;
    auto* central = new QWidget;
    mainWindow.setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);

    // --- MruTabWidget demo ---
    auto* tabGroup = new QGroupBox("MruTabWidget  (Ctrl+Tab = MRU navigation)");
    auto* tabGroupLayout = new QVBoxLayout(tabGroup);

    auto* tabWidget = new MruTabWidget;
    tabWidget->setTabsClosable(true);
    tabWidget->setMinimalTabCount(1);

    const QStringList tabNames = {"main.cpp", "widget.h", "CMakeLists.txt", "README.md", "dialog.cpp"};
    for (const QString& name : tabNames) {
        auto* editor = new QTextEdit;
        editor->setPlainText(
            QString("// %1\n\nPress Ctrl+Tab to navigate with MRU popup.\n"
                    "Right-click a tab for context menu (pin, close, etc.)").arg(name));
        tabWidget->addTab(editor, name);
    }
    tabGroupLayout->addWidget(tabWidget);
    mainLayout->addWidget(tabGroup, 2);

    // --- QxFileDialog / QxDirDialog demo ---
    auto* dialogGroup = new QGroupBox("QxFileDialog / QxDirDialog");
    auto* dialogLayout = new QVBoxLayout(dialogGroup);

    auto* btnRow = new QHBoxLayout;
    auto* resultRow = new QHBoxLayout;

    QStringList recentFiles;
    QStringList recentDirs;
    const QString home = QDir::homePath();

    // Open file
    auto* openBtn = new QPushButton("Open File...");
    auto* openLabel = new QLabel("(no file)");
    openLabel->setWordWrap(true);
    QObject::connect(openBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString f = QxFileDialog::getOpenFileName(
            &mainWindow, "Open File", home, "All Files (*)", &recentFiles);
        if (!f.isEmpty()) openLabel->setText(f);
    });

    // Save file
    auto* saveBtn = new QPushButton("Save File...");
    auto* saveLabel = new QLabel("(no file)");
    saveLabel->setWordWrap(true);
    QObject::connect(saveBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString f = QxFileDialog::getSaveFileName(
            &mainWindow, "Save File", home, "Text Files (*.txt);;All Files (*)", &recentFiles);
        if (!f.isEmpty()) saveLabel->setText(f);
    });

    // Select directory
    auto* dirBtn = new QPushButton("Select Directory...");
    auto* dirLabel = new QLabel("(no directory)");
    dirLabel->setWordWrap(true);
    QObject::connect(dirBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString d = QxDirDialog::getExistingDirectory(
            &mainWindow, "Select Directory", home, &recentDirs);
        if (!d.isEmpty()) dirLabel->setText(d);
    });

    btnRow->addWidget(openBtn);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(dirBtn);
    resultRow->addWidget(openLabel);
    resultRow->addWidget(saveLabel);
    resultRow->addWidget(dirLabel);

    dialogLayout->addLayout(btnRow);
    dialogLayout->addLayout(resultRow);
    mainLayout->addWidget(dialogGroup);

    mainWindow.resize(860, 620);
    mainWindow.setWindowTitle("qt-extra demo");
    mainWindow.show();

    return app.exec();
}