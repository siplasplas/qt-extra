#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QDir>
#include <QFileDialog>

#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxrecentdialog.h"
#include "qxfilebreadcrumb.h"

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
    demoTabs->setMinimalTabCount(3);
    mainLayout->addWidget(demoTabs);

    auto* mruPage = new QWidget;
    auto* mruPageLayout = new QVBoxLayout(mruPage);
    demoTabs->addTab(mruPage, "MruTabWidget");

    auto* fileDialogPage = new QWidget;
    auto* fileDialogPageLayout = new QVBoxLayout(fileDialogPage);
    demoTabs->addTab(fileDialogPage, "QxFileDialog");

    auto* breadcrumbPage = new QWidget;
    auto* breadcrumbPageLayout = new QVBoxLayout(breadcrumbPage);
    demoTabs->addTab(breadcrumbPage, "QxFileBreadcrumb");

    // --- MruTabWidget demo ---
    auto* tabGroup = new QGroupBox("Ctrl+Tab = MRU navigation");
    auto* tabGroupLayout = new QVBoxLayout(tabGroup);

    auto* firstTabs = new MruTabWidget;
    auto* secondTabs = new MruTabWidget;
    for (auto* tabs : {firstTabs, secondTabs}) {
        tabs->setTabsClosable(true);
        tabs->setMinimalTabCount(1);
        tabs->setMovable(true);
    }

    auto addDemoTab = [](MruTabWidget* tabs, const QString& title, const QString& path) {
        auto* editor = new QTextEdit;
        editor->setPlainText(
            QString("// %1\n\nPress Ctrl+Tab to navigate with MRU popup.\n"
                    "The popup shows the full path \"%2\" instead of the tab title.\n"
                    "Drag tabs to move them. Right-click a tab to pin or close it.")
                .arg(title, path));
        const int index = tabs->addTab(editor, title);
        tabs->setTabPopupText(index, path);
    };
    addDemoTab(firstTabs, "main.cpp", "demo/main.cpp");
    addDemoTab(firstTabs, "mrutabwidget.h", "widgets/mrutabwidget.h");
    addDemoTab(firstTabs, "CMakeLists.txt", "CMakeLists.txt");
    addDemoTab(secondTabs, "README.md", "README.md");
    addDemoTab(secondTabs, "qxfiledialog.cpp", "widgets/qxfiledialog.cpp");
    addDemoTab(secondTabs, "qxrecentdialog.cpp", "widgets/qxrecentdialog.cpp");

    auto* optionsRow = new QHBoxLayout;
    optionsRow->addWidget(new QLabel("Tab position:"));
    auto* positionCombo = new QComboBox;
    positionCombo->addItems({"Top", "Bottom"});
    optionsRow->addWidget(positionCombo);
    auto* movableCheck = new QCheckBox("Movable tabs");
    movableCheck->setChecked(true);
    optionsRow->addWidget(movableCheck);
    auto* sequentialCheck = new QCheckBox("Sequential Ctrl+Tab");
    optionsRow->addWidget(sequentialCheck);
    auto* swapButton = new QPushButton("Swap selected tabs");
    optionsRow->addWidget(swapButton);
    optionsRow->addStretch();
    tabGroupLayout->addLayout(optionsRow);

    QObject::connect(positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [=](int index) {
        const auto position = index == 0 ? QTabWidget::North : QTabWidget::South;
        firstTabs->setTabPosition(position);
        secondTabs->setTabPosition(position);
    });
    QObject::connect(movableCheck, &QCheckBox::toggled, [=](bool movable) {
        firstTabs->setMovable(movable);
        secondTabs->setMovable(movable);
    });
    QObject::connect(sequentialCheck, &QCheckBox::toggled, [=](bool sequential) {
        firstTabs->setSequentialTabSwitching(sequential);
        secondTabs->setSequentialTabSwitching(sequential);
    });
    QObject::connect(swapButton, &QPushButton::clicked, [=]() {
        firstTabs->swapExternal(secondTabs, firstTabs->currentIndex(), secondTabs->currentIndex());
    });

    auto* tabSetsRow = new QHBoxLayout;
    for (const auto& tabSet : {qMakePair(firstTabs, QString("Set A")), qMakePair(secondTabs, QString("Set B"))}) {
        auto* setGroup = new QGroupBox(tabSet.second);
        auto* setLayout = new QVBoxLayout(setGroup);
        setLayout->addWidget(tabSet.first);
        tabSetsRow->addWidget(setGroup, 1);
    }
    tabGroupLayout->addLayout(tabSetsRow, 1);
    mruPageLayout->addWidget(tabGroup);

    // --- Filesystem breadcrumb demo ---
    auto* breadcrumbGroup = new QGroupBox("Filesystem breadcrumb");
    auto* breadcrumbGroupLayout = new QVBoxLayout(breadcrumbGroup);
    breadcrumbGroupLayout->addWidget(new QLabel(
        "Click a path segment to go there; click > to choose a subdirectory."));
    auto* breadcrumb = new QxFileBreadcrumb;
    breadcrumbGroupLayout->addWidget(breadcrumb);
    auto* directoryList = new QListWidget;
    breadcrumbGroupLayout->addWidget(directoryList, 1);
    breadcrumbPageLayout->addWidget(breadcrumbGroup);

    auto refreshDirectory = [breadcrumb, directoryList]() {
        directoryList->clear();
        const QDir dir(breadcrumb->path());
        const QFileInfoList entries = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
            QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& entry : entries)
            directoryList->addItem(entry.fileName() + (entry.isDir() ? "/" : ""));
    };
    QObject::connect(breadcrumb, &QxFileBreadcrumb::pathActivated,
                     [refreshDirectory](const QString&) { refreshDirectory(); });
    refreshDirectory();

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

    auto* saveAsCheck = new QCheckBox("Save as");
    fileLayout->addWidget(saveAsCheck);

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
        if (!saveAsCheck->isChecked()) {
            log("open native", QFileDialog::getOpenFileName(&mainWindow, "Open File", home, fileFilter));
        } else {
            log("save native", QFileDialog::getSaveFileName(&mainWindow, "Save File", home, fileFilter));
        }
    });

    QObject::connect(fileQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QFileDialog dlg(&mainWindow, !saveAsCheck->isChecked() ? "Open File" : "Save File", home, fileFilter);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        if (!saveAsCheck->isChecked()) {
            dlg.setFileMode(QFileDialog::ExistingFile);
        } else {
            dlg.setAcceptMode(QFileDialog::AcceptSave);
        }
        const QString tag = !saveAsCheck->isChecked() ? "open qt" : "save qt";
        if (dlg.exec() == QDialog::Accepted)
            log(tag, dlg.selectedFiles().first());
        else
            log(tag);
    });

    QObject::connect(fileDialogBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (!saveAsCheck->isChecked()) {
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
        if (!saveAsCheck->isChecked()) {
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
