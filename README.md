# qt-extra

Extra Qt widgets and utilities designed to be included as a Git submodule.

## Components

### MruTabWidget
`QTabWidget` extension with MRU (Most Recently Used) tab navigation via Ctrl+Tab,
tab pinning, configurable tab limits, and context-sensitive close buttons.

### QxFileDialog
Full file-system browser dialog (open/save/directory) with a tree view,
places sidebar, navigation history, an editable path bar, and name filters.
A drop-in alternative to `QFileDialog` with a custom, controllable UI.

### QxRecentFileDialog
Lightweight file open/save dialog that displays a built-in recently-used
files list. The caller owns and persists the list — pass it in, get the
updated list back.

### QxDirDialog
Directory selection dialog with a built-in recently-used directories list.
Same ownership model as `QxRecentFileDialog`.

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
git submodule add https://github.com/youruser/qt-extra.git extern/qt-extra
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
#include "qxrecentfiledialog.h"
#include "qxdirdialog.h"
```

The demo application is **not** built when qt-extra is included as a submodule.
To force-enable it: `-DQT_EXTRA_BUILD_DEMO=ON`.

## Building standalone (demo)

```bash
cmake -B build
cmake --build build
./build/demo/qt-extra-demo
```

## License

MIT — see [LICENSE](LICENSE).