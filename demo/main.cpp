#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QDir>
#include <QFileDialog>

#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxdirdialog.h"
#include "qxfilebrowser.h"

static const char* fileFilter =
    "All Files (*);;Code (*.cpp *.c *.h);;Images (*.jpg *.png *.gif)";

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

    // --- File dialog demo ---
    const QString home = QDir::homePath();
    QStringList recentDirs;

    auto* fileGroup = new QGroupBox("File");
    auto* fileLayout = new QVBoxLayout(fileGroup);

    auto* modeRow = new QHBoxLayout;
    auto* openRadio = new QRadioButton("Open");
    auto* saveRadio = new QRadioButton("Save");
    openRadio->setChecked(true);
    modeRow->addWidget(openRadio);
    modeRow->addWidget(saveRadio);
    modeRow->addStretch();
    fileLayout->addLayout(modeRow);

    auto* fileBtnRow = new QHBoxLayout;
    auto* fileNativeBtn = new QPushButton("native");
    auto* fileQtBtn     = new QPushButton("qt");
    auto* fileCustomBtn = new QPushButton("custom");
    fileBtnRow->addWidget(fileNativeBtn);
    fileBtnRow->addWidget(fileQtBtn);
    fileBtnRow->addWidget(fileCustomBtn);
    fileBtnRow->addStretch();
    fileLayout->addLayout(fileBtnRow);

    QObject::connect(fileNativeBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (openRadio->isChecked()) {
            log("open native", QFileDialog::getOpenFileName(&mainWindow, "Open File", home, fileFilter));
        } else {
            log("save native", QFileDialog::getSaveFileName(&mainWindow, "Save File", home, fileFilter));
        }
    });

    QObject::connect(fileQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QFileDialog dlg(&mainWindow, openRadio->isChecked() ? "Open File" : "Save File", home, fileFilter);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        if (openRadio->isChecked()) {
            dlg.setFileMode(QFileDialog::ExistingFile);
        } else {
            dlg.setAcceptMode(QFileDialog::AcceptSave);
        }
        const QString tag = openRadio->isChecked() ? "open qt" : "save qt";
        if (dlg.exec() == QDialog::Accepted)
            log(tag, dlg.selectedFiles().first());
        else
            log(tag);
    });

    QObject::connect(fileCustomBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (openRadio->isChecked()) {
            log("open custom", QxFileBrowser::getOpenFileName(
                &mainWindow, "Open File", home, fileFilter));
        } else {
            log("save custom", QxFileBrowser::getSaveFileName(
                &mainWindow, "Save File", home, fileFilter));
        }
    });

    // --- Directory dialog demo ---
    auto* dirGroup = new QGroupBox("Directory");
    auto* dirLayout = new QVBoxLayout(dirGroup);

    auto* dirBtnRow = new QHBoxLayout;
    auto* dirNativeBtn = new QPushButton("native");
    auto* dirQtBtn     = new QPushButton("qt");
    auto* dirCustomBtn = new QPushButton("custom");
    dirBtnRow->addWidget(dirNativeBtn);
    dirBtnRow->addWidget(dirQtBtn);
    dirBtnRow->addWidget(dirCustomBtn);
    dirBtnRow->addStretch();
    dirLayout->addLayout(dirBtnRow);

    QObject::connect(dirNativeBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        log("dir native", QFileDialog::getExistingDirectory(&mainWindow, "Select Directory", home));
    });

    QObject::connect(dirQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QFileDialog dlg(&mainWindow, "Select Directory", home);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        dlg.setFileMode(QFileDialog::Directory);
        if (dlg.exec() == QDialog::Accepted)
            log("dir qt", dlg.selectedFiles().first());
        else
            log("dir qt");
    });

    QObject::connect(dirCustomBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        log("dir custom", QxDirDialog::getExistingDirectory(
            &mainWindow, "Select Directory", home, &recentDirs));
    });

    auto* dialogRowLayout = new QHBoxLayout;
    dialogRowLayout->addWidget(fileGroup, 2);
    dialogRowLayout->addWidget(dirGroup, 1);

    auto* dialogGroup = new QGroupBox("Dialogs");
    auto* dialogLayout = new QVBoxLayout(dialogGroup);
    dialogLayout->addLayout(dialogRowLayout);
    mainLayout->addWidget(dialogGroup);

    auto* logGroup = new QGroupBox("Log");
    auto* logLayout = new QVBoxLayout(logGroup);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(logGroup);

    mainWindow.resize(860, 680);
    mainWindow.setWindowTitle("qt-extra demo");
    mainWindow.show();

    return app.exec();
}