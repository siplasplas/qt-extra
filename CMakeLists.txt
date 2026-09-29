cmake_minimum_required(VERSION 3.16)
project(qt-extra VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Demo defaults to ON when built standalone, OFF when included as submodule
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(QT_EXTRA_BUILD_DEMO "Build qt-extra demo application" ON)
else()
    option(QT_EXTRA_BUILD_DEMO "Build qt-extra demo application" OFF)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_library(qt-extra STATIC
    common/Ev.cpp
    widgets/mrutabwidget.cpp
    widgets/qxrecentdialog.cpp
    widgets/qxrecentdialog.h
    widgets/qxfiledialog.cpp
    widgets/qxbreadcrumb.cpp
    widgets/qxfilebreadcrumb.cpp
)

target_include_directories(qt-extra
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/widgets>
        $<INSTALL_INTERFACE:include/qt-extra>
)

target_link_libraries(qt-extra PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

# Install rules (optional, useful even for submodule users who install)
install(TARGETS qt-extra
    EXPORT   qt-extra-targets
    ARCHIVE  DESTINATION lib
)
install(FILES
    common/Ev.h
    widgets/mrutabwidget.h
    widgets/qxrecentdialog.h
    widgets/qxfiledialog.h
    widgets/qxbreadcrumb.h
    widgets/qxfilebreadcrumb.h
    DESTINATION include/qt-extra
)
install(EXPORT qt-extra-targets
    FILE        qt-extra-config.cmake
    DESTINATION lib/cmake/qt-extra
)

if(QT_EXTRA_BUILD_DEMO)
    add_subdirectory(demo)
endif()
