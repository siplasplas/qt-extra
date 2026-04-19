add_executable(qt-extra-demo main.cpp)

target_link_libraries(qt-extra-demo PRIVATE qt-extra)

set_target_properties(qt-extra-demo PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE    TRUE
)