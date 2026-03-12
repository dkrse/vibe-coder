cmake_minimum_required(VERSION 3.16)
project(vibe-coder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

add_executable(vibe-coder
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/terminalwidget.cpp
    src/terminalwidget.h
    src/filebrowser.cpp
    src/filebrowser.h
    src/settings.cpp
    src/settings.h
    src/codeeditor.cpp
    src/codeeditor.h
    src/promptedit.cpp
    src/promptedit.h
    src/project.cpp
    src/project.h
    src/projectdialog.cpp
    src/projectdialog.h
)

# Link qtermwidget manually to avoid missing libutf8proc.so symlink
target_include_directories(vibe-coder PRIVATE /usr/include)
target_link_libraries(vibe-coder PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui
    /usr/lib/x86_64-linux-gnu/libqtermwidget6.so.2
)
