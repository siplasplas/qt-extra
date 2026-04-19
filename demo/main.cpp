#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QDir>
#include <QFileDialog>

#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxdirdialog.h"
#include "qxfilebrowser.h"

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

    // --- File / Dir dialog demo ---
    auto* dialogGroup = new QGroupBox("File / Directory selection");
    auto* dialogLayout = new QVBoxLayout(dialogGroup);

    QStringList recentFiles;
    QStringList recentDirs;
    const QString home = QDir::homePath();

    // --- Log ---
    auto* logEdit = new QPlainTextEdit;
    logEdit->setReadOnly(true);
    logEdit->setMaximumHeight(120);
    logEdit->setPlaceholderText("log...");
    auto log = [logEdit](const QString& action, const QString& path = {}) {
        if (path.isEmpty())
            logEdit->appendPlainText(action + ": cancel");
        else
            logEdit->appendPlainText(action + ": accept  " + path);
    };

    // Open file — 3 ways
    auto* openGroup = new QGroupBox("Open file");
    auto* openLayout = new QVBoxLayout(openGroup);
    auto* openBtnRow = new QHBoxLayout;

    auto* openNativeBtn  = new QPushButton("Open (native)");
    auto* openQtBtn      = new QPushButton("Open (Qt)");
    auto* openCustomBtn  = new QPushButton("Open (custom)");

    QObject::connect(openNativeBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString f = QFileDialog::getOpenFileName(
            &mainWindow, "Open File (native)", home, "All Files (*)");
        log("open native", f);
    });

    QObject::connect(openQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QFileDialog dlg(&mainWindow, "Open File (Qt)", home, "All Files (*)");
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        dlg.setFileMode(QFileDialog::ExistingFile);
        if (dlg.exec() == QDialog::Accepted)
            log("open qt", dlg.selectedFiles().first());
        else
            log("open qt");
    });

    QObject::connect(openCustomBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString f = QxFileBrowser::getOpenFileName(
            &mainWindow, "Open File (custom)", home, "All Files (*)");
        log("open custom", f);
    });

    openBtnRow->addWidget(openNativeBtn);
    openBtnRow->addWidget(openQtBtn);
    openBtnRow->addWidget(openCustomBtn);
    openLayout->addLayout(openBtnRow);

    // Save file
    auto* saveGroup = new QGroupBox("Save file");
    auto* saveLayout = new QVBoxLayout(saveGroup);
    auto* saveBtn = new QPushButton("Save File...");
    QObject::connect(saveBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString f = QxFileDialog::getSaveFileName(
            &mainWindow, "Save File", home, "Text Files (*.txt);;All Files (*)", &recentFiles);
        log("save", f);
    });
    saveLayout->addWidget(saveBtn);

    // Select directory
    auto* dirGroup = new QGroupBox("Select directory");
    auto* dirLayout = new QVBoxLayout(dirGroup);
    auto* dirBtn = new QPushButton("Select Directory...");
    QObject::connect(dirBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString d = QxDirDialog::getExistingDirectory(
            &mainWindow, "Select Directory", home, &recentDirs);
        log("dir", d);
    });
    dirLayout->addWidget(dirBtn);

    auto* dialogRowLayout = new QHBoxLayout;
    dialogRowLayout->addWidget(openGroup, 3);
    dialogRowLayout->addWidget(saveGroup, 1);
    dialogRowLayout->addWidget(dirGroup, 1);

    dialogLayout->addLayout(dialogRowLayout);
    mainLayout->addWidget(dialogGroup);

    auto* logGroup = new QGroupBox("Log");
    auto* logGroupLayout = new QVBoxLayout(logGroup);
    logGroupLayout->addWidget(logEdit);
    mainLayout->addWidget(logGroup);

    mainWindow.resize(860, 680);
    mainWindow.setWindowTitle("qt-extra demo");
    mainWindow.show();

    return app.exec();
}