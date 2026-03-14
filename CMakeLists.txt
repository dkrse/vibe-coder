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
    src/sshdialog.cpp
    src/sshdialog.h
    src/sshmanager.cpp
    src/sshmanager.h
    src/sshtunneldialog.cpp
    src/sshtunneldialog.h
    src/commandpalette.cpp
    src/commandpalette.h
    src/notificationpanel.cpp
    src/notificationpanel.h
    src/diffviewer.cpp
    src/diffviewer.h
    src/changesmonitor.cpp
    src/changesmonitor.h
    src/gitgraph.cpp
    src/gitgraph.h
    src/titlebar.cpp
    src/titlebar.h
    src/zedthemeloader.cpp
    src/zedthemeloader.h
    src/themeddialog.cpp
    src/themeddialog.h
)

# Find qtermwidget library (works on Fedora, Debian/Ubuntu, Arch, etc.)
find_library(QTERMWIDGET_LIB
    NAMES qtermwidget6 qtermwidget6.so.2
    HINTS /usr/lib64 /usr/lib/x86_64-linux-gnu /usr/lib
)
if(NOT QTERMWIDGET_LIB)
    message(FATAL_ERROR "libqtermwidget6 not found. Install qtermwidget-devel (Fedora) or libqtermwidget6-1-dev (Debian/Ubuntu).")
endif()
message(STATUS "Found qtermwidget: ${QTERMWIDGET_LIB}")

target_include_directories(vibe-coder PRIVATE /usr/include)
target_link_libraries(vibe-coder PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui
    ${QTERMWIDGET_LIB}
)
