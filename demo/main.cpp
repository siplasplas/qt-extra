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
#include "qxrecentdialog.h"

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
    auto* demoTabs = new MruTabWidget(central);
    demoTabs->setMinimalTabCount(2);
    mainLayout->addWidget(demoTabs);

    auto* mruPage = new QWidget;
    auto* mruPageLayout = new QVBoxLayout(mruPage);
    demoTabs->addTab(mruPage, "MruTabWidget");

    auto* fileDialogPage = new QWidget;
    auto* fileDialogPageLayout = new QVBoxLayout(fileDialogPage);
    demoTabs->addTab(fileDialogPage, "QxFileDialog");

    // --- MruTabWidget demo ---
    auto* tabGroup = new QGroupBox("Ctrl+Tab = MRU navigation");
    auto* tabGroupLayout = new QVBoxLayout(tabGroup);

    auto* tabWidget = new MruTabWidget;
    tabWidget->setTabsClosable(true);
    tabWidget->setMinimalTabCount(1);

    // Short tab title + a longer path shown only in the Ctrl+Tab popup.
    const QList<QPair<QString, QString>> tabInfo = {
        {"main.cpp",       "demo/main.cpp"},
        {"widget.h",       "widgets/mrutabwidget.h"},
        {"CMakeLists.txt", "CMakeLists.txt"},
        {"README.md",      "docs/README.md"},
        {"dialog.cpp",     "widgets/qxfiledialog.cpp"},
    };
    for (const auto& info : tabInfo) {
        auto* editor = new QTextEdit;
        editor->setPlainText(
            QString("// %1\n\nPress Ctrl+Tab to navigate with MRU popup.\n"
                    "The popup shows the full path \"%2\" instead of the tab title.\n"
                    "Right-click a tab for context menu (pin, close, etc.)")
                .arg(info.first, info.second));
        int idx = tabWidget->addTab(editor, info.first);
        tabWidget->setTabPopupText(idx, info.second);
    }
    tabGroupLayout->addWidget(tabWidget);
    mruPageLayout->addWidget(tabGroup);

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
    QStringList fileHistory, dirHistory, fileRecent, dirRecent;
    auto addToHistory = [](QStringList& list, const QString& path) {
        if (path.isEmpty()) return;
        list.removeAll(path);
        list.prepend(path);
    };

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
    auto* fileNativeBtn  = new QPushButton("native");
    auto* fileQtBtn      = new QPushButton("Qt");
    auto* fileDialogBtn = new QPushButton("custom");
    auto* fileRecentBtn  = new QPushButton("recent files");
    fileBtnRow->addWidget(fileNativeBtn);
    fileBtnRow->addWidget(fileQtBtn);
    fileBtnRow->addWidget(fileDialogBtn);
    fileBtnRow->addWidget(fileRecentBtn);
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

    QObject::connect(fileDialogBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (openRadio->isChecked()) {
            QString f = QxFileDialog::getOpenFileName(
                &mainWindow, "Open File", home, fileFilter, fileHistory);
            addToHistory(fileHistory, f);
            log("open browser", f);
        } else {
            QString f = QxFileDialog::getSaveFileName(
                &mainWindow, "Save File", home, fileFilter, "untitled.txt", fileHistory);
            addToHistory(fileHistory, f);
            log("save browser", f);
        }
    });

    QObject::connect(fileRecentBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (openRadio->isChecked()) {
            QString f = QxRecentDialog::getOpenFileName(
                &mainWindow, "Open File", home, fileFilter, &fileRecent);
            log("open dialog", f);
        } else {
            QString f = QxRecentDialog::getSaveFileName(
                &mainWindow, "Save File", home, fileFilter, &fileRecent);
            log("save dialog", f);
        }
    });

    // --- Directory dialog demo ---
    auto* dirGroup = new QGroupBox("Directory");
    auto* dirLayout = new QVBoxLayout(dirGroup);

    auto* dirBtnRow = new QHBoxLayout;
    auto* dirNativeBtn = new QPushButton("native");
    auto* dirQtBtn     = new QPushButton("qt");
    auto* dirCustomBtn = new QPushButton("custom");
    auto* dirRecentBtn = new QPushButton("recent directories");
    dirBtnRow->addWidget(dirNativeBtn);
    dirBtnRow->addWidget(dirQtBtn);
    dirBtnRow->addWidget(dirCustomBtn);
    dirBtnRow->addWidget(dirRecentBtn);
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
        QString d = QxFileDialog::getExistingDirectory(
            &mainWindow, "Select Directory", home, dirHistory);
        addToHistory(dirHistory, d);
        log("dir custom", d);
    });

    QObject::connect(dirRecentBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QString d = QxRecentDialog::getExistingDirectory(
            &mainWindow, "Select Directory", home, &dirRecent);
        log("dir recent", d);
    });

    auto* dialogRowLayout = new QHBoxLayout;
    dialogRowLayout->addWidget(fileGroup, 2);
    dialogRowLayout->addWidget(dirGroup, 1);

    auto* dialogGroup = new QGroupBox("Dialogs");
    auto* dialogLayout = new QVBoxLayout(dialogGroup);
    dialogLayout->addLayout(dialogRowLayout);
    fileDialogPageLayout->addWidget(dialogGroup);

    auto* logGroup = new QGroupBox("Log");
    auto* logLayout = new QVBoxLayout(logGroup);
    logLayout->addWidget(logEdit);
    fileDialogPageLayout->addWidget(logGroup);

    mainWindow.resize(860, 680);
    mainWindow.setWindowTitle("qt-extra demo");
    mainWindow.show();

    return app.exec();
}
