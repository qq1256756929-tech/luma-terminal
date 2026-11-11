cmake_minimum_required(VERSION 3.22)

project(LumaTerm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg)

qt_standard_project_setup(REQUIRES 6.8)

# ---------------------------------------------------------------------------
# libghostty-vt: Ghostty's terminal parser/state library (MIT).
# The shared library and headers are vendored so the app has no external
# terminal-emulation dependency.
# ---------------------------------------------------------------------------
set(GHOSTTY_VT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libghostty)
add_library(ghostty_vt SHARED IMPORTED)
set_target_properties(ghostty_vt PROPERTIES
    IMPORTED_LOCATION ${GHOSTTY_VT_ROOT}/lib/libghostty-vt.so.0.1.0
    INTERFACE_INCLUDE_DIRECTORIES ${GHOSTTY_VT_ROOT}/include
)

qt_add_executable(luma
    src/main.cpp
    src/Theme.cpp
    src/TerminalSession.cpp
    src/TerminalView.cpp
    src/TerminalManager.cpp
)

target_include_directories(luma PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(luma PRIVATE
    LUMA_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
    LUMA_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    LUMA_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(luma PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    ghostty_vt
    util
)

set_target_properties(luma PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH ${GHOSTTY_VT_ROOT}/lib
    INSTALL_RPATH "\$ORIGIN/../lib"
)

install(TARGETS luma RUNTIME DESTINATION bin)
install(DIRECTORY qml DESTINATION share/luma)
install(DIRECTORY assets DESTINATION share/luma)
install(DIRECTORY ${GHOSTTY_VT_ROOT}/lib/ DESTINATION lib)
install(FILES packaging/luma.desktop DESTINATION share/applications)
install(FILES assets/luma.png DESTINATION share/icons/hicolor/512x512/apps)
install(FILES assets/luma.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES README.md LICENSE DESTINATION share/doc/luma)
install(DIRECTORY docs DESTINATION share/doc/luma)
