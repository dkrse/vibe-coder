# Architecture

## Overview

Vibe Coder is a Qt6 C++ IDE-like application designed for AI-assisted development workflows. It combines a file browser, code editor, dual terminal emulators, SSH remote development, and prompt system into a single window.

## Technology Stack

- **Language:** C++17
- **UI Framework:** Qt6 Widgets
- **Terminal:** QTermWidget (libqtermwidget6)
- **SSH Filesystem:** sshfs / FUSE
- **Build System:** CMake 3.16+
- **Settings:** QSettings (INI-based, per-user)

## Application Structure

```
src/
├── main.cpp              Entry point, QApplication + MainWindow
├── mainwindow.h/cpp      Central orchestrator, menu, splitters, session, SSH coordination
├── filebrowser.h/cpp     Zed-style file tree with async git status, dual model (local/SSH)
├── codeeditor.h/cpp      Text editor with line numbers + syntax highlighting
├── terminalwidget.h/cpp  QTermWidget wrapper for embedded terminal
├── promptedit.h/cpp      Prompt input with configurable send key + save-and-send
├── settings.h/cpp        AppSettings struct, SettingsDialog, ColorButton
├── project.h/cpp         .LLM/instructions.json project management + saved prompts
├── projectdialog.h/cpp   Create/Edit project dialog
├── sshdialog.h/cpp       SSH connection dialog with saved connections
├── sshmanager.h/cpp      Central SSH management: profiles, health, transfers, tunnels
└── sshtunneldialog.h/cpp Port forwarding management dialog
```

## Component Responsibilities

### MainWindow
- Top-level layout: horizontal splitter (file browser | right panel)
- Right panel: vertical splitter (tab widget | bottom tab widget)
- Tab widget: AI-terminal (tab 0, non-closable) + editor tabs
- Bottom tab widget: Prompt tab + Terminal tab
- Hamburger menu (top-right): Create/Edit Project, SSH Connect/Disconnect/Tunnels/Upload/Download, Settings
- Status bar: file info, SSH profile combo, transfer progress bar
- Session persistence: window geometry, splitter states, open files
- File watcher: auto-reload externally changed files
- Keyboard shortcuts: Ctrl+S save
- Tab close properly deletes CodeEditor widget (no memory leak)
- Async git commit via chained QProcess callbacks

### FileBrowser
- Dual model: QFileSystemModel for local, QStandardItemModel for SSH (synchronous QDir reads over sshfs)
- QTreeView with custom FileItemDelegate for Zed editor-style rendering
- File type colors, directory arrows, git status colors (modified, untracked, added)
- Async git status via 3-step QProcess pipeline (non-blocking, 5s interval)
- Directory git status via precomputed cache (O(1) lookup)
- Context menu: New File, New Directory, Rename, Delete
- SSH path translation: toRemotePath() maps mount paths to remote paths
- Path bar accepts remote paths when SSH is active
- Dark/Light theme support
- Path traversal protection in file operations

### CodeEditor
- QPlainTextEdit subclass with LineNumberArea widget
- SyntaxHighlighter: C/C++, Python, JavaScript/TypeScript, Rust
- Dark/Light color scheme with matching syntax colors
- Single rehighlight pass: setLanguage() builds rules, setDarkTheme() triggers rehighlight
- Configurable line number visibility

### TerminalWidget
- Thin wrapper around QTermWidget
- Shell started after widget is placed in layout (deferred startShellProgram)
- sendText() appends \r for proper Enter simulation
- Supports interactive apps (claude, mc, vim, etc.)
- Font and color scheme configurable via Settings

### PromptEdit
- QPlainTextEdit subclass with custom keyPressEvent
- Configurable send key: Enter or Ctrl+Enter
- Shift modifier: send + save prompt
- Two signals: sendRequested() and saveAndSendRequested()

### SshManager
- Central SSH management replacing scattered SSH state
- **Profile management:** add/remove/connect/disconnect multiple simultaneous profiles
- **Async mount:** sshfs mount runs asynchronously via QProcess signals (no UI blocking)
- **Health check:** async sequential check of each profile's mount (stat with 3s timeout)
- **Auto-reconnect:** up to 3 attempts on connection loss, async fusermount + remount
- **File transfer:** upload/download via sshfs mount point (rsync --progress or cp)
- **Port forwarding:** SSH tunnels (-L/-R) with SSH_ASKPASS for password auth
- **Security:** input validation against shell metacharacters, password never in process args
- **Cached rsync detection:** `which rsync` called once, result cached

### SshTunnelDialog
- QDialog for creating/viewing SSH port forwards
- Form: name, direction (Local -L / Remote -R), local port, remote host, remote port
- QTableWidget showing active tunnels with Remove buttons
- Auto-refreshes on tunnel start/stop signals

### Project System
- Project data stored in `.LLM/instructions.json`
- Fields: project_name, project_hash (SHA256), description, model, git_remote
- Prompt history with sequential ID, timestamp, model info
- Saved/recurring prompts stored as IDs referencing the prompts array
- Auto-creates `.gitignore` with `.LLM/` entry
- Async git commit with automatic sensitive file filtering

### Settings
- AppSettings struct with load/save via QSettings("vibe-coder", "vibe-coder")
- Sections: Terminal, Editor, File Browser, Prompt
- SettingsDialog with grouped QFormLayout sections
- ColorButton widget with QColorDialog picker

## Data Flow

1. **File Opening:** FileBrowser emits `fileOpened` -> MainWindow creates CodeEditor tab, adds to FileWatcher
2. **Prompt Sending:** PromptEdit emits `sendRequested` -> MainWindow logs to Project + sends to Terminal
3. **Save+Send:** PromptEdit emits `saveAndSendRequested` -> MainWindow saves prompt ID + sends to Terminal
4. **Git Commit:** Commit button -> async QProcess chain: init -> add -A -> commit (non-blocking)
5. **Settings:** SettingsDialog -> MainWindow applies to all components (language set before theme for single rehighlight)
6. **Git Status:** Timer (5s) -> async QProcess pipeline -> cache rebuild -> viewport update
7. **SSH Connect:** SshDialog -> MainWindow adds profile -> SshManager async mount -> profileConnected signal -> setup file browser + terminals
8. **SSH Reconnect:** Health timer -> async stat check -> connectionLost -> async fusermount + remount -> reconnected
9. **File Transfer:** Upload/Download via sshfs mount (rsync --progress or cp) -> transferProgress/transferFinished signals
10. **Tab Close:** removeTab + widget->deleteLater() prevents memory leak

## Session Persistence

Stored in QSettings on application close:
- Window geometry and state
- Main splitter and right splitter positions
- File browser root path
- List of open file paths
- Active tab index

## Security Model

- SSH passwords: sent via PTY (echo off) for terminals, password_stdin for sshfs, SSH_ASKPASS for tunnels
- Passwords never persisted to disk (QSettings stores connections without password)
- Passwords never appear in process argument lists (no sshpass -p)
- SSH user/host validated against shell metacharacters before use
- Git commit auto-adds .env, *.pem, *.key, credentials.json to .gitignore
- Path traversal protection in file browser operations
- StrictHostKeyChecking=accept-new for SSH connections
