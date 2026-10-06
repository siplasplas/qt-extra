#include <QApplication>
#include <QLineEdit>
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
#include <QFileInfo>
#include <QTimer>
#include <QSpinBox>
#include <QMessageBox>
#include <memory>

#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxrecentdialog.h"
#include "qxfilebreadcrumb.h"

static const char* fileFilter =
    "All Files (*);;Code (*.cpp *.c *.h);;Images (*.jpg *.png *.gif);;"
    "Audio (*.wav *.mp3 *.ogg *.oga *.opus *.flac)";

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

    auto demoText = [](const QString& title, const QString& path) {
        return QString("// %1\n\nPress Ctrl+Tab to navigate with MRU popup.\n"
                       "The popup shows the full path \"%2\" instead of the tab title.\n"
                       "Drag tabs to move them. Right-click a tab to pin or close it.")
            .arg(title, path);
    };
    auto addDemoTab = [demoText](MruTabWidget* tabs, const QString& title, const QString& path) {
        auto* editor = new QTextEdit;
        editor->setPlainText(demoText(title, path));
        const int index = tabs->addTab(editor, title);
        tabs->setTabPopupText(index, path);
        tabs->setTabKey(editor, path);
        return editor;
    };
    addDemoTab(firstTabs, "main.cpp", "demo/main.cpp");
    QWidget* attentionTab = addDemoTab(firstTabs, "mrutabwidget.h", "widgets/mrutabwidget.h");
    addDemoTab(firstTabs, "CMakeLists.txt", "CMakeLists.txt");
    QWidget* pinnedTab = addDemoTab(secondTabs, "README.md", "README.md");
    QWidget* busyTab = addDemoTab(secondTabs, "qxfiledialog.cpp", "widgets/qxfiledialog.cpp");
    addDemoTab(secondTabs, "qxrecentdialog.cpp", "widgets/qxrecentdialog.cpp");
    secondTabs->setTabPinned(pinnedTab, true);
    secondTabs->setTabBusy(busyTab, true);
    firstTabs->setTabAttention(attentionTab, true);

    // Opens a file in Set A: an open file is activated, otherwise it replaces the preview tab
    const QStringList previewFiles = {"widgets/qxbreadcrumb.h", "widgets/qxfilebreadcrumb.h",
                                      "common/Ev.h", "LICENSE"};
    auto previewFile = [=](const QString& path) {
        if (QWidget* open = firstTabs->findTab(path)) {
            firstTabs->setCurrentWidget(open);
            return;
        }
        const QString title = QFileInfo(path).fileName();
        auto* editor = qobject_cast<QTextEdit*>(firstTabs->previewTab());
        if (!editor) {
            editor = new QTextEdit;
            firstTabs->addTab(editor, title);
            firstTabs->setTabPreview(editor, true);
        }
        const int index = firstTabs->indexOf(editor);
        editor->setPlainText(demoText(title, path)
                             + "\n\nThis is the preview tab: double-click it to keep it open.");
        firstTabs->setTabText(index, title);
        firstTabs->setTabPopupText(index, path);
        firstTabs->setTabKey(editor, path);
        firstTabs->setCurrentWidget(editor);
    };
    previewFile(previewFiles.first());
    firstTabs->setCurrentIndex(0);

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

    auto* stateRow = new QHBoxLayout;
    stateRow->addWidget(new QLabel("Set A:"));
    auto* previewCombo = new QComboBox;
    previewCombo->addItems(previewFiles);
    stateRow->addWidget(previewCombo);
    auto* previewButton = new QPushButton("Preview");
    stateRow->addWidget(previewButton);
    auto* busyButton = new QPushButton("Toggle busy");
    stateRow->addWidget(busyButton);
    auto* attentionButton = new QPushButton("Attention on others in 2 s");
    stateRow->addWidget(attentionButton);
    stateRow->addStretch();
    tabGroupLayout->addLayout(stateRow);

    auto* limitRow = new QHBoxLayout;
    limitRow->addWidget(new QLabel("Tab limit:"));
    auto* limitSpin = new QSpinBox;
    limitSpin->setRange(0, 20);
    limitSpin->setSpecialValueText("unlimited");
    limitSpin->setToolTip("Unpinned tabs per set; the least recently used ones are closed");
    limitRow->addWidget(limitSpin);
    auto* askPinCheck = new QCheckBox("Ask which tab gives way");
    askPinCheck->setChecked(true);
    limitRow->addWidget(askPinCheck);
    auto* newTabAButton = new QPushButton("New tab in A");
    limitRow->addWidget(newTabAButton);
    auto* newTabBButton = new QPushButton("New tab in B");
    limitRow->addWidget(newTabBButton);
    limitRow->addStretch();
    tabGroupLayout->addLayout(limitRow);

    auto newTabCounter = std::make_shared<int>(0);
    auto addNewTab = [=](MruTabWidget* tabs) {
        // Ask before adding, so that Cancel can still keep the new tab out
        if (!tabs->makeRoomForNewTab())
            return;
        const QString title = QString("new%1.txt").arg(++*newTabCounter);
        tabs->setCurrentWidget(addDemoTab(tabs, title, "new/" + title));
    };
    QObject::connect(newTabAButton, &QPushButton::clicked, [=]() { addNewTab(firstTabs); });
    QObject::connect(newTabBButton, &QPushButton::clicked, [=]() { addNewTab(secondTabs); });
    QObject::connect(limitSpin, QOverload<int>::of(&QSpinBox::valueChanged), [=](int limit) {
        firstTabs->setTabLimit(limit);
        secondTabs->setTabLimit(limit);
    });

    QObject::connect(previewButton, &QPushButton::clicked, [=]() {
        previewFile(previewCombo->currentText());
    });
    QObject::connect(busyButton, &QPushButton::clicked, [=]() {
        if (QWidget* page = firstTabs->currentWidget())
            firstTabs->setTabBusy(page, !firstTabs->isTabBusy(page));
    });
    QObject::connect(attentionButton, &QPushButton::clicked, [=]() {
        QTimer::singleShot(2000, firstTabs, [=]() {
            for (int i = 0; i < firstTabs->count(); ++i)
                firstTabs->setTabAttention(firstTabs->widget(i), true);
        });
    });

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

    auto* tabLog = new QPlainTextEdit;
    tabLog->setReadOnly(true);
    tabLog->setMaximumHeight(80);
    tabLog->setPlaceholderText("tab signals...");
    tabGroupLayout->addWidget(tabLog);
    for (auto* tabs : {firstTabs, secondTabs}) {
        QObject::connect(tabs, &MruTabWidget::tabClosing, [=](QWidget* page) {
            tabLog->appendPlainText("closing " + tabs->tabText(tabs->indexOf(page)));
        });
        QObject::connect(tabs, &MruTabWidget::previewTabPromoted, [=](QWidget* page) {
            tabLog->appendPlainText("promoted " + tabs->tabText(tabs->indexOf(page)));
        });
        // The tab whose fate was already decided in tabLimitReached; its close is not asked again
        auto decided = std::make_shared<QPointer<QWidget>>();
        // Direct connection: action is read right after the signal returns
        QObject::connect(tabs, &MruTabWidget::tabLimitReached,
                         [=, &mainWindow](QWidget* page, MruTabWidget::LimitAction& action) {
            *decided = page;
            if (!askPinCheck->isChecked())
                return;
            const QString title = tabs->tabText(tabs->indexOf(page));
            QMessageBox box(QMessageBox::Question, "Tab limit reached",
                            QString("The new tab needs room. What should happen to \"%1\"?").arg(title),
                            QMessageBox::NoButton, &mainWindow);
            QAbstractButton* close = box.addButton("Close it", QMessageBox::AcceptRole);
            QAbstractButton* keep = box.addButton("Keep it (pin)", QMessageBox::ActionRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(qobject_cast<QPushButton*>(close));
            box.exec();
            if (box.clickedButton() == close)
                return;
            action = box.clickedButton() == keep ? MruTabWidget::LimitAction::Keep
                                                 : MruTabWidget::LimitAction::Cancel;
            tabLog->appendPlainText((action == MruTabWidget::LimitAction::Keep
                                         ? "kept and pinned " : "new tab cancelled for ") + title);
        });
        // Lowering the limit closes tabs without a new one; a veto pins the tab
        QObject::connect(tabs, &MruTabWidget::tabAboutToClose,
                         [=, &mainWindow](QWidget* page, bool askPin, bool& allow) {
            if (page == *decided) {
                *decided = nullptr;
                return;
            }
            if (!askPin || !askPinCheck->isChecked())
                return;
            const QString title = tabs->tabText(tabs->indexOf(page));
            const auto answer = QMessageBox::question(
                &mainWindow, "Tab limit lowered",
                QString("Close \"%1\"?\nNo keeps it open and pins it.").arg(title));
            if (answer != QMessageBox::Yes) {
                allow = false;
                tabLog->appendPlainText("kept and pinned " + title);
            }
        });
        QObject::connect(tabs, &MruTabWidget::tabContextMenuRequested, [=](QWidget*, QMenu* menu) {
            QAction* first = menu->actions().value(0);
            QAction* newTab = new QAction("New Tab", menu);
            QObject::connect(newTab, &QAction::triggered, [=]() { addNewTab(tabs); });
            menu->insertAction(first, newTab);
            menu->insertSeparator(first);
        });
    }
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
    auto* multipleCheck = new QCheckBox("Select multiple files (Open only)");
    fileLayout->addWidget(multipleCheck);
    QObject::connect(saveAsCheck, &QCheckBox::toggled, multipleCheck, [multipleCheck](bool save) {
        multipleCheck->setEnabled(!save);
    });
    auto* defaultFileRow = new QHBoxLayout;
    auto* defaultFileEdit = new QLineEdit;
    defaultFileEdit->setPlaceholderText("Default file (name, relative or absolute path)");
    auto* lastFileBtn = new QPushButton("Use last selected");
    lastFileBtn->setEnabled(false);
    defaultFileRow->addWidget(defaultFileEdit, 1);
    defaultFileRow->addWidget(lastFileBtn);
    fileLayout->addLayout(defaultFileRow);
    QString lastSelectedFile;
    QObject::connect(lastFileBtn, &QPushButton::clicked, [&, defaultFileEdit] {
        defaultFileEdit->setText(lastSelectedFile);
    });
    auto* audioMetadataCheck = new QCheckBox("Show duration");
    audioMetadataCheck->setToolTip("Audio duration for WAV, MP3 and Vorbis/Opus; unknown stays blank.");
    auto* imageMetadataCheck = new QCheckBox("Show size");
    imageMetadataCheck->setToolTip("Image width and height in encoded pixels; depends on Qt image plugins.");
    fileLayout->addWidget(audioMetadataCheck);
    fileLayout->addWidget(imageMetadataCheck);
    QString browserDirectory = home;
    auto recordFiles = [&](const QString& action, const QStringList& files) {
        if (files.isEmpty()) {
            log(action);
            return;
        }
        lastSelectedFile = files.first();
        lastFileBtn->setEnabled(true);
        for (const QString& file : files) {
            addToHistory(fileHistory, file);
            log(action, file);
        }
    };

    auto* fileBtnRow = new QHBoxLayout;
    auto* fileNativeBtn  = new QPushButton("native");
    auto* fileQtBtn      = new QPushButton("Qt");
    auto* fileDialogBtn = new QPushButton("custom");
    fileDialogBtn->setToolTip("Drag column header separators to resize; double-click to fit contents.\n"
                              "Typing a name quick-searches the list (substring); Up/Down jump between "
                              "matches, Right/Tab complete the name.");
    auto* fileRecentBtn  = new QPushButton("recent files");
    fileBtnRow->addWidget(fileNativeBtn);
    fileBtnRow->addWidget(fileQtBtn);
    fileBtnRow->addWidget(fileDialogBtn);
    fileBtnRow->addWidget(fileRecentBtn);
    fileBtnRow->addStretch();
    fileLayout->addLayout(fileBtnRow);

    QObject::connect(fileNativeBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        const QString initial = defaultFileEdit->text().trimmed().isEmpty()
            ? browserDirectory : QDir(browserDirectory).filePath(defaultFileEdit->text().trimmed());
        if (!saveAsCheck->isChecked()) {
            if (multipleCheck->isChecked()) {
                recordFiles("open native", QFileDialog::getOpenFileNames(&mainWindow, "Open Files", initial, fileFilter));
            } else {
                const QString file = QFileDialog::getOpenFileName(&mainWindow, "Open File", initial, fileFilter);
                recordFiles("open native", file.isEmpty() ? QStringList{} : QStringList{file});
            }
        } else {
            const QString file = QFileDialog::getSaveFileName(&mainWindow, "Save File", initial, fileFilter);
            recordFiles("save native", file.isEmpty() ? QStringList{} : QStringList{file});
        }
    });

    QObject::connect(fileQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QFileDialog dlg(&mainWindow, !saveAsCheck->isChecked() ? "Open File" : "Save File", browserDirectory, fileFilter);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        if (!saveAsCheck->isChecked()) {
            dlg.setFileMode(multipleCheck->isChecked() ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
        } else {
            dlg.setAcceptMode(QFileDialog::AcceptSave);
        }
        if (!defaultFileEdit->text().trimmed().isEmpty()) {
            const QFileInfo initial(QDir(browserDirectory).filePath(defaultFileEdit->text().trimmed()));
            dlg.setDirectory(initial.absolutePath());
            dlg.selectFile(initial.fileName());
        }
        const QString tag = !saveAsCheck->isChecked() ? "open qt" : "save qt";
        if (dlg.exec() == QDialog::Accepted)
            recordFiles(tag, dlg.selectedFiles());
        else
            log(tag);
    });

    QObject::connect(fileDialogBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QxFileDialog dlg(&mainWindow, saveAsCheck->isChecked() ? QxFileDialog::Save : QxFileDialog::Open);
        dlg.setDirectory(browserDirectory);
        dlg.setNameFilter(fileFilter);
        dlg.setHistory(fileHistory);
        dlg.setMultipleSelectionEnabled(multipleCheck->isChecked());
        dlg.setAudioDurationVisible(audioMetadataCheck->isChecked());
        dlg.setImageDimensionsVisible(imageMetadataCheck->isChecked());
        if (!defaultFileEdit->text().trimmed().isEmpty()) dlg.setFileName(defaultFileEdit->text().trimmed());
        else if (saveAsCheck->isChecked()) dlg.setFileName("untitled.txt");
        const QStringList files = dlg.exec() == QDialog::Accepted ? dlg.selectedFiles() : QStringList{};
        browserDirectory = dlg.directory();
        recordFiles(saveAsCheck->isChecked() ? "save browser" : "open browser", files);
        log("last browser directory", browserDirectory);
    });

    QObject::connect(fileRecentBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        if (!saveAsCheck->isChecked()) {
            QString f = QxRecentDialog::getOpenFileName(
                &mainWindow, "Open File", home, fileFilter, &fileRecent);
            recordFiles("open dialog", f.isEmpty() ? QStringList{} : QStringList{f});
        } else {
            QString f = QxRecentDialog::getSaveFileName(
                &mainWindow, "Save File", home, fileFilter, &fileRecent);
            recordFiles("save dialog", f.isEmpty() ? QStringList{} : QStringList{f});
        }
    });

    // --- Directory dialog demo ---
    auto* dirGroup = new QGroupBox("Directory");
    auto* dirLayout = new QVBoxLayout(dirGroup);
    auto* defaultDirEdit = new QLineEdit;
    defaultDirEdit->setPlaceholderText("Default directory (absolute or relative path)");
    dirLayout->addWidget(defaultDirEdit);
    QString directoryBrowserPath = home;

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
        const QString initial = QDir(directoryBrowserPath).filePath(defaultDirEdit->text().trimmed());
        log("dir native", QFileDialog::getExistingDirectory(&mainWindow, "Select Directory", initial));
    });

    QObject::connect(dirQtBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        const QString initial = QDir(directoryBrowserPath).filePath(defaultDirEdit->text().trimmed());
        QFileDialog dlg(&mainWindow, "Select Directory", initial);
        dlg.setOption(QFileDialog::DontUseNativeDialog, true);
        dlg.setFileMode(QFileDialog::Directory);
        if (dlg.exec() == QDialog::Accepted)
            log("dir qt", dlg.selectedFiles().first());
        else
            log("dir qt");
    });

    QObject::connect(dirCustomBtn, &QPushButton::clicked, [&, &mainWindow = mainWindow]() {
        QxFileDialog dlg(&mainWindow, QxFileDialog::Directory);
        dlg.setDirectory(directoryBrowserPath);
        dlg.setHistory(dirHistory);
        if (!defaultDirEdit->text().trimmed().isEmpty()) dlg.setFileName(defaultDirEdit->text().trimmed());
        const QString d = dlg.exec() == QDialog::Accepted ? dlg.selectedFile() : QString{};
        directoryBrowserPath = dlg.directory();
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
