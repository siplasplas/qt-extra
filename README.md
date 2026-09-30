# qt-extra

Extra Qt widgets and utilities designed to be included as a Git submodule.

## Components

### MruTabWidget
`QTabWidget` extension for IDE-style tab sets: MRU (Most Recently Used) navigation
with Ctrl+Tab, pinned tabs, a limit on open tabs, an optional preview tab, tab keys and
busy/attention markers.

All per-tab state (pinned, preview, busy, attention, key, MRU position) belongs to the
page widget, not to the tab index, so it follows a tab when the user drags it or when
`swapTabs()`/`swapExternal()` move it. Signals identify tabs by page (`QWidget *`); an
index is valid only at the moment it is read, so client code should keep pages instead.

#### Navigation
- **Ctrl+Tab** switches to the previous tab in MRU order when released quickly; held
  longer (or pressed again) it shows a popup listing the tabs, most recent first.
  Ctrl+Shift+Tab moves backwards, releasing Ctrl activates the selected tab, Escape
  cancels. `setSequentialTabSwitching(true)` makes Ctrl+Tab cycle left to right instead.
- `setTabPopupText(index, text)` sets a longer text for the popup (e.g. a full path)
  while the tab shows only its title.

#### Closing tabs
Use `requestCloseTab(page)` (or the index overload), not `removeTab()`; also do not
connect to `QTabWidget::tabCloseRequested`. A close goes through these steps:

1. `tabAboutToClose(page, askPin, allow)`: a receiver can set `allow` to false to keep the
   tab, e.g. to ask about saving. `allow` is read right after emission, so receivers
   must use a direct connection (the default in the same thread). With several
   receivers `allow` may already be false.
2. `tabClosing(page)`: closing was allowed; the page is still in the widget. Do cleanup
   here (e.g. save state, move a shared view out of the page). Do not delete the page.
3. The tab is removed, then `tabClosed(page)` is emitted.
4. The page is deleted with `deleteLater()`, unless `setDeletePagesOnClose(false)` was
   called; then the client owns it after `tabClosed`.

`setMinimalTabCount(n)` keeps at least `n` tabs open: closing is refused and the close
actions are disabled at that count (`canCloseTabs()`). `requestCloseAllTabs()`,
`closeOtherTabs()`, `closeTabsToLeft()` and `closeTabsToRight()` close several tabs;
each one still asks `tabAboutToClose`.

#### Tab limit and pinning
- `setTabLimit(n)` limits the number of unpinned tabs (0 = unlimited). After a tab is
  added, `enforceTabLimit()` closes the least recently used unpinned tabs; it can also be
  called directly and returns how many tabs it closed.
- These closes emit `tabAboutToClose` with `askPin = true`. A receiver that vetoes such a
  close (e.g. the user answered "keep it") gets the tab pinned instead.
- Pinned tabs never count toward the limit and are never closed by it. A pinned tab
  shows a pin in place of its close button; clicking the pin unpins the tab. The pin
  icon is built in; `setPinIconUri()` replaces it.
- `setTabPinned(page, bool)` / `isTabPinned(page)` (also by index).

#### Context menu
Right-clicking a tab shows Close, Close Other Tabs, Close All Tabs (only when the
minimal tab count is 0), Close Tabs to the Left/Right and Pin/Unpin Tab.
`tabContextMenuRequested(page, menu)` is emitted before the menu is shown, so the client
can add its own actions (capture the page, not an index).

#### Optional features
A client that does not use them sees the plain behavior.

- **Preview tab** (IDE style, like VS Code's italic tab). `setTabPreview(page, true)`
  marks a tab as a preview: its title is drawn in italics. There is at most one; setting
  a new one clears the old flag. The preview tab does not count toward the tab limit and
  is never closed by `enforceTabLimit()`. It becomes a regular tab when the user
  double-clicks it, when the client calls `promotePreviewTab()` (e.g. when the user
  starts editing the previewed item) or when it is pinned; `previewTabPromoted(page)` is
  then emitted. `previewTab()` returns the current preview page or nullptr. The italics
  come from a proxy of the application style that is set on the tab bar the first time
  a preview tab is set.
- **Tab keys**. `setTabKey(page, key)` stores a client-defined key, e.g. a file path or a
  conversation id; `findTab(key)` returns the page showing it, so an already open item
  can be activated instead of opened twice.
- **Markers**. `setTabBusy(page, true)` shows a spinner, e.g. while a background job or
  an agent is still working. `setTabAttention(page, true)` shows a dot on a background
  tab with new content; it is cleared when the tab becomes current, and setting it on
  the current tab does nothing. Markers use the tab button on the side opposite the close
  button, so they never replace the close or pin button; they are not shown when the
  client put its own button on that side.

#### Example
```cpp
auto *tabs = new MruTabWidget;
tabs->setTabsClosable(true);
tabs->setMovable(true);
tabs->setTabLimit(10);

connect(tabs, &MruTabWidget::tabAboutToClose,
        [](QWidget *page, bool askPin, bool &allow) {
    if (auto *editor = qobject_cast<Editor *>(page); editor && editor->isModified())
        allow = askToSave(editor);  // askPin: a refusal pins the tab
});
connect(tabs, &MruTabWidget::tabClosing, [](QWidget *page) { rememberRecent(page); });

// Single click: show the file in the preview tab
void openPreview(const QString &path) {
    if (QWidget *open = tabs->findTab(path)) { tabs->setCurrentWidget(open); return; }
    QWidget *page = tabs->previewTab();
    if (!page) {
        page = createEditor();
        tabs->addTab(page, QString());
        tabs->setTabPreview(page, true);
    }
    load(page, path);
    tabs->setTabText(tabs->indexOf(page), QFileInfo(path).fileName());
    tabs->setTabKey(page, path);
    tabs->setCurrentWidget(page);
}
```

#### Migrating from 1.x
Version 2.0.0 changes the signals to take the page:

| 1.x | 2.0 |
|-----|-----|
| `tabAboutToClose(int index, bool askPin, bool &allow_close)` | `tabAboutToClose(QWidget *page, bool askPin, bool &allow)` |
| `actionsBeforeTabClose(int index)` | `tabClosing(QWidget *page)` |
| `tabContextMenuRequested(int tabIndex, QMenu *menu)` | `tabContextMenuRequested(QWidget *page, QMenu *menu)` |
| — | `tabClosed(QWidget *page)`, `previewTabPromoted(QWidget *page)` |

Replace `widget(index)` lookups in receivers with the page argument, and do not keep an
index in actions or lambdas. Index-based methods remain; `requestCloseTab`,
`setTabPinned` and `isTabPinned` also take a page.

Behavior changes: pinned, closed and context-menu state now follows moved tabs (in 1.x
it stayed at the old index, so after a move the wrong tab was pinned or closed); pinned
tabs keep their place in the Ctrl+Tab MRU order; a pinned tab shows a pin icon by
default.

### QxFileDialog
Full file-system browser dialog (open/save/directory) with a tree view,
places sidebar, navigation history, a filesystem breadcrumb, and name filters.
A drop-in alternative to `QFileDialog` with a custom, controllable UI.

Since 2.1.0, `setAudioDurationVisible(true)` adds a numeric-sortable Duration
column (`03:12.4`, rounded to tenths; minutes may exceed 59).
`setImageDimensionsVisible(true)` independently adds numeric-sortable Width
and Height columns. Both options default to false: disabled features do no
metadata reads or background scans. `directory()` returns the last browsed
directory after either acceptance or cancellation. Existing static helpers
keep their signatures and default behavior.

Audio inspection reads binary headers without decoding or launching players:
PCM/IEEE-float RIFF WAV (including extra chunks and padding), MPEG audio frames
(including variable bitrate MP3), and single-stream Ogg Vorbis/Opus are supported.
MP3 and Ogg scans are limited to 128 MiB; larger files, unsupported/chained Ogg,
compressed WAV, corrupt/truncated files and unavailable metadata stay blank.
MP3 durations count every audio frame and validate Xing/Info or VBRI frame
counts where present, rather than estimating from a bitrate; encoder
delay/padding is not subtracted. Ogg Opus subtracts its pre-skip.
Directories sort first and unknown values last in either sort direction; Size
sorting remains numeric. Click column headers to switch sorting.

Image dimensions are encoded pixel sizes, before EXIF rotation, obtained with
`QImageReader::size()` without a full-image decode. PNG/JPEG/GIF/BMP and other
formats depend on installed Qt handlers; unavailable dimensions stay blank.
Only audio extensions listed above and image extensions advertised by the
installed Qt handlers are probed; other extensions (including extensionless
images) stay blank.
Metadata jobs run in the background (at most two per dialog), only for requested
rows in the current directory. Results are cached by path, size and modification
time. Navigation, hiding the dialog or disabling a feature cancels work and
removes metadata file watches. Enabled metadata file watches also invalidate
results when contents change without a directory notification. Cancellation
discards stale results; an ongoing filesystem read or image-handler size call
may finish before noticing cancellation. Neither sorting nor painting waits
for metadata I/O.

```cpp
QxFileDialog dialog(parent, QxFileDialog::Open);
dialog.setDirectory(lastAudioDirectory);
dialog.setNameFilter("Sounds (*.wav *.mp3 *.ogg *.oga *.opus)");
dialog.setAudioDurationVisible(true);
const bool accepted = dialog.exec() == QDialog::Accepted;
lastAudioDirectory = dialog.directory(); // Persist on cancel too.
if (accepted) useSound(dialog.selectedFile());
```

The demo window offers separate "Show duration" (audio) and "Show size" (image
width/height) checkboxes, both off by default. These controls are outside the
file chooser; its normal Name, Size (file bytes) and Date Modified columns stay
visible in open/save mode. The demo remembers the last directory
on both outcomes. Manually check header sorting, navigation, F2 rename, new
folders, and pasted absolute/relative paths. After installing this static
library, rebuild/relink agentdeskt and gemini-commander to use the new code;
clients using `find_package(qt-extra 2 REQUIRED)` remain source-compatible.

- Right-click a file or folder to rename it in place (also F2); right-click
  anywhere for **New > Folder**, which creates `new_folder` (or `new_folder(1)`,
  `new_folder(2)`, ...) and starts renaming it right away.
- Drag the separators in the column header to resize any visible column,
  including Name and the optional metadata columns. Double-click a separator
  to fit that column to its contents.
- The file name field accepts relative or absolute paths (`/`, and on Windows
  also `\` and `C:`). Directory parts are entered and removed from the field.
  An existing file is accepted at once; a path ending in a directory only
  navigates there, so a second Enter chooses it. A name filled in by selecting
  an item in the list is accepted as is.

### QxRecentDialog
Lightweight file open/save and directory selection dialog with a built-in
recently-used paths list. The caller owns and persists the list — pass it in,
get the updated list back. Keep separate lists for files and directories.

### QxBreadcrumb
General breadcrumb driven by caller-supplied segments. It emits signals when a
segment is clicked or its menu needs actions.

### QxFileBreadcrumb
Filesystem breadcrumb built on `QxBreadcrumb`. Click a path segment to navigate
to it, or click the arrow beside it to choose one of its subdirectories.

### Ev (common utility)
Qt5/Qt6 compatible helpers for extracting local and global positions from mouse,
hover, and wheel events.

## Requirements

- CMake 3.16+
- Qt 5.15+ or Qt 6.x (auto-detected)
- C++17

## Using as a Git submodule

**1. Add the submodule to your project:**
```bash
git submodule add https://github.com/siplasplas/qt-extra.git extern/qt-extra
git submodule update --init
```

**2. In your `CMakeLists.txt`:**
```cmake
add_subdirectory(extern/qt-extra)
target_link_libraries(my-app PRIVATE qt-extra)
```

Headers are exposed automatically — no extra `include_directories()` needed:
```cpp
#include "mrutabwidget.h"
#include "qxfiledialog.h"
#include "qxrecentdialog.h"
#include "qxbreadcrumb.h"
#include "qxfilebreadcrumb.h"
```

The demo application is **not** built when qt-extra is included as a submodule.
To force-enable it: `-DQT_EXTRA_BUILD_DEMO=ON`.

## Installing

```bash
cmake -B build
cmake --build build
sudo cmake --install build
```

This installs the static library, the headers under `include/qt-extra` and a CMake
package with a version file (`SameMajorVersion`):

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(qt-extra 2 REQUIRED)
target_link_libraries(my-app PRIVATE qt-extra)
```

## Building standalone (demo)

```bash
cmake -B build
cmake --build build
./build/demo/qt-extra-demo
```

The demo's MruTabWidget page has two tab sets with a pinned, a busy, an attention and a
preview tab, a tab limit spin box (with a prompt that pins a tab instead of closing it),
buttons and a context menu entry for new tabs, and a log of the close signals.

## License

MIT — see [LICENSE](LICENSE).
