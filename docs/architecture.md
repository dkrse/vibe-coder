# Architecture

## Overview

Vibe Coder is a Qt6 C++ IDE-like application designed for AI-assisted development workflows. It combines a file browser, code editor, terminal emulator, and prompt system into a single window.

## Technology Stack

- **Language:** C++17
- **UI Framework:** Qt6 Widgets
- **Terminal:** QTermWidget (libqtermwidget6)
- **Build System:** CMake 3.16+
- **Settings:** QSettings (INI-based, per-user)

## Application Structure

```
src/
├── main.cpp            Entry point, QApplication + MainWindow
├── mainwindow.h/cpp    Central orchestrator, menu, splitters, session
├── filebrowser.h/cpp   Zed-style file tree with async git status
├── codeeditor.h/cpp    Text editor with line numbers + syntax highlighting
├── terminalwidget.h/cpp  QTermWidget wrapper for embedded terminal
├── promptedit.h/cpp    Prompt input with configurable send key
├── settings.h/cpp      AppSettings struct, SettingsDialog, ColorButton
├── project.h/cpp       .LLM/instructions.json project management
└── projectdialog.h/cpp Create/Edit project dialog
```

## Component Responsibilities

### MainWindow
- Top-level layout: horizontal splitter (file browser | right panel)
- Right panel: vertical splitter (tab widget | prompt area)
- Tab widget: terminal (tab 0, non-closable) + editor tabs
- Hamburger menu (top-right corner): Create/Edit Project, Settings
- Session persistence: window geometry, splitter states, open files
- File watcher: auto-reload externally changed files
- Keyboard shortcuts: Ctrl+S save

### FileBrowser
- QFileSystemModel + QTreeView with custom FileItemDelegate
- Zed editor-style rendering: file type colors, directory arrows
- Async git status via 3-step QProcess pipeline (non-blocking)
- Directory git status via precomputed cache (O(1) lookup)
- Context menu: New File, New Directory, Rename, Delete
- Dark/Light theme support
- Path traversal protection in file operations

### CodeEditor
- QPlainTextEdit subclass with LineNumberArea widget
- SyntaxHighlighter: C/C++, Python, JavaScript/TypeScript, Rust
- Dark/Light color scheme with matching syntax colors
- Configurable line number visibility

### TerminalWidget
- Thin wrapper around QTermWidget(1) (immediate shell start)
- sendText() appends \r for proper Enter simulation
- Supports interactive apps (claude, mc, vim, etc.)
- Font and color scheme configurable via Settings

### PromptEdit
- QPlainTextEdit subclass with custom keyPressEvent
- Configurable send key: Enter or Ctrl+Enter
- Shift+Enter always inserts newline (in Enter mode)

### Project System
- Project data stored in `.LLM/instructions.json`
- Fields: project_name, project_hash (SHA256), description, model, git_remote
- Prompt history with sequential ID, timestamp, model info
- Auto-creates `.gitignore` with `.LLM/` entry
- Git commit integration via QProcess

### Settings
- AppSettings struct with load/save via QSettings("vibe-coder", "vibe-coder")
- Sections: Terminal, Editor, File Browser, Prompt
- SettingsDialog with grouped QFormLayout sections
- ColorButton widget with QColorDialog picker

## Data Flow

1. **File Opening:** FileBrowser emits `fileOpened` -> MainWindow creates CodeEditor tab
2. **Prompt Sending:** PromptEdit emits `sendRequested` -> MainWindow logs to Project + sends to Terminal
3. **Git Commit:** Commit button -> MainWindow runs git init/add/commit via QProcess
4. **Settings:** SettingsDialog -> MainWindow applies to all components
5. **Git Status:** Timer (2s) -> async QProcess pipeline -> cache rebuild -> viewport update

## Session Persistence

Stored in QSettings on application close:
- Window geometry and state
- Main splitter and right splitter positions
- File browser root path
- List of open file paths
- Active tab index
