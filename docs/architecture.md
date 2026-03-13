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
├── settings.h/cpp        AppSettings struct, SettingsDialog (tabbed), ColorButton, global themes
├── project.h/cpp         .LLM/instructions.json project management + saved prompts
├── projectdialog.h/cpp   Create/Edit project dialog
├── sshdialog.h/cpp       SSH connection dialog with saved connections
├── sshmanager.h/cpp      Central SSH management: profiles, health, transfers, tunnels
├── sshtunneldialog.h/cpp Port forwarding management dialog
├── commandpalette.h/cpp  VS Code-style command palette (Ctrl+Shift+P)
├── diffviewer.h/cpp      Git diff viewer with syntax highlighting
├── changesmonitor.h/cpp  Real-time file change tracker with diff preview + revert
└── notificationpanel.h/cpp  Log panel with Info/Warning/Error/Success levels
```

## Component Responsibilities

### MainWindow
- Top-level layout: horizontal splitter (file browser | right panel)
- Right panel: vertical splitter (editor splitter | bottom tab widget)
- Editor splitter: wraps main tab widget, supports horizontal/vertical split view for side-by-side editing
- Tab widget: AI-terminal (tab 0, non-closable) + editor tabs
- Bottom tab widget: Prompt tab + Terminal tab + Notifications tab + Diff Viewer tab + Changes tab
- Hamburger menu (top-right): Create/Edit Project, SSH Connect/Disconnect/Tunnels/Upload/Download, Split Horizontal/Vertical/Unsplit, Settings
- Command palette (Ctrl+Shift+P): fuzzy-searchable list of all registered commands (split, focus, theme switching, diff refresh)
- Global theme system: Dark, Light, Monokai, Solarized Dark, Solarized Light, Nord — applies QSS stylesheet for all chrome
- Status bar: file info, SSH profile combo, transfer progress bar
- Session persistence: window geometry, splitter states, open files
- File watcher: auto-reload externally changed files (300ms debounce)
- Keyboard shortcuts: Ctrl+S save, Ctrl+F find, Ctrl+H find & replace, Ctrl+Shift+P command palette
- **Stop button** — sends configurable stop sequence to terminal (default: `\x03`), supports escape sequences
- **Unsaved changes tracking** — `●` tab marker, Save/Discard/Cancel on tab close, Save All/Discard/Cancel on app close
- **Tab context menu** — Close, Close Others, Close All, Close to Right, Close to Left (all check unsaved changes)
- Tab close properly deletes CodeEditor widget (no memory leak)
- Async git commit via chained QProcess callbacks
- Notification system: centralized logging with unread badge count
- Changes monitor integration: tracks file changes per project, badge count on tab

### FileBrowser
- Dual model: QFileSystemModel for local, QStandardItemModel for SSH (synchronous QDir reads over sshfs)
- QTreeView with custom FileItemDelegate for Zed editor-style rendering
- File type colors, directory arrows, git status colors (modified, untracked, added, ignored)
- Async git status via 3-step QProcess pipeline (non-blocking, 10s interval, includes `--ignored`)
- Ignored files: dimmed color rendering (dark: #5a5a5a, light: #b0b0b0)
- Directory git status via precomputed cache (O(1) lookup) — covers modified, untracked, added, ignored
- Visibility filtering via `FileBrowserProxy` (QSortFilterProxyModel):
  - Gitignored files: visible / grayed / hidden (configurable in Settings > Visibility)
  - .git directory: visible / grayed / hidden (default: hidden)
  - Uses `beginFilterChange()/endFilterChange()` to preserve tree expansion state and scroll position
  - SSH model filters in `sshPopulateDir()`
- Context menu: New File, New Directory, Rename, Delete
- SSH path translation: toRemotePath() maps mount paths to remote paths
- Path bar accepts remote paths when SSH is active
- Dark/Light theme support
- Path traversal protection in file operations
- Git polling timer only starts after git repo confirmed, stops when no repo

### CodeEditor
- QPlainTextEdit subclass with LineNumberArea widget
- SyntaxHighlighter: C/C++, Python, JavaScript/TypeScript, Rust + search pattern overlay
- Dark/Light color scheme with matching syntax colors
- Single rehighlight pass: setLanguage() builds rules, setDarkTheme() triggers rehighlight
- Configurable line number visibility
- **Find & Replace** (FindReplaceBar widget):
  - Ctrl+F opens find bar, Ctrl+H opens find & replace
  - Yellow highlight of all matches via syntax highlighter overlay
  - Yellow markers on vertical scrollbar via MarkerScrollBar (QScrollBar subclass)
  - Match counter ("3 of 12"), navigate with Enter/Shift+Enter or arrows
  - Replace one / Replace All with undo group support
  - Theme-aware (dark/light) via `setDarkTheme(bool)`
- **Undo/Redo** — built-in QPlainTextEdit support (Ctrl+Z / Ctrl+Shift+Z)
- **Current line highlighting** — configurable via Settings > Editor
- **Unsaved changes tracking** — compares `toPlainText()` against saved content, shows `●` marker in tab title
- **Search debouncing** — 200ms delay before scanning document on keystroke
- **Optimized line number painting** — reuses QString via `setNum()`, caches font metrics and area width

### TerminalWidget
- Thin wrapper around QTermWidget
- Shell started after widget is placed in layout (deferred startShellProgram)
- sendText() appends \r for proper Enter simulation
- Supports interactive apps (claude, mc, vim, etc.)
- Font and color scheme configurable via Settings
- Font applied via `setFamily()` + `setStyleHint(QFont::Monospace)`, reapplied after event loop via `QTimer::singleShot(0)` to override QTermWidget reset

### PromptEdit
- QPlainTextEdit subclass with custom keyPressEvent
- Configurable send key: Enter or Ctrl+Enter
- Shift modifier: send + save prompt
- Two signals: sendRequested() and saveAndSendRequested()
- **Current line highlighting** — configurable via Settings > Prompt, auto-detects dark/light from palette

### SshManager
- Central SSH management replacing scattered SSH state
- **Profile management:** add/remove/connect/disconnect multiple simultaneous profiles
- **Async mount:** sshfs mount runs asynchronously via QProcess signals (no UI blocking)
- **Health check:** async sequential check of each profile's mount (stat with 3s timeout, 15s interval)
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

### CommandPalette
- Overlay widget with search input + filtered list of commands
- Commands registered with name, shortcut string, and action callback
- Fuzzy filtering on keystroke, Enter to execute selected command
- Event filter handles Escape to dismiss, arrow keys for navigation
- Built-in commands: split/unsplit, focus panels, theme switching, diff refresh

### DiffViewer
- QPlainTextEdit with DiffHighlighter (added/removed/hunk header coloring)
- Mode combo for switching diff views (e.g. staged vs unstaged)
- Refresh button triggers `git diff` on current working directory
- Configurable font and colors via Settings (diff section)
- Embeds as a tab in the bottom tab widget

### ChangesMonitor
- Real-time file change tracker using QFileSystemWatcher + periodic scan (10s, only when visible)
- Split layout: file list (left, max 300px) + diff preview (right) with DiffBlockHighlighter
- Toolbar: info label, Open, Revert, Refresh, Clear buttons
- Watches up to 2000 files and 500 directories (skips .git, .LLM, node_modules, __pycache__)
- Deduplicates changes (same file replaces previous entry, newest first)
- 1-second debounce to avoid duplicate signals
- Revert: runs `git checkout -- <file>` asynchronously, marks entry as reverted
- Open: emits `openFileRequested` signal to open file in editor
- Diff preview: runs `git diff` for tracked files, shows content for new files
- Re-adds watched files after signal (QFileSystemWatcher removes on some platforms)
- Badge count on bottom tab: "Changes (N)"
- Configurable font and colors via Settings (changes section)

### NotificationPanel
- Timestamped log entries with 4 severity levels: Info, Warning, Error, Success
- QListWidget with color-coded entries and level icons
- Unread count with `unreadChanged` signal for badge updates
- Clear button to reset the log

### Settings
- AppSettings struct with load/save via QSettings("vibe-coder", "vibe-coder")
- **Global theme:** Light (default), Dark, Monokai, Solarized Dark, Solarized Light, Nord — `applyThemeDefaults()` sets component colors (editor, browser, terminal scheme, prompt, diff, changes)
- Theme change cascades: SettingsDialog combo updates dependent fields live; MainWindow applies QSS stylesheet for all chrome (tabs, splitters, status bar, menus, buttons)
- Tabbed SettingsDialog: Global Theme (top), Terminal, Editor, File Browser, Prompt, Diff Viewer, Changes Monitor, Visibility tabs
- ColorButton widget with QColorDialog picker

## Data Flow

1. **File Opening:** FileBrowser emits `fileOpened` -> MainWindow creates CodeEditor tab, adds to FileWatcher
2. **Prompt Sending:** PromptEdit emits `sendRequested` -> MainWindow logs to Project + sends to Terminal
3. **Save+Send:** PromptEdit emits `saveAndSendRequested` -> MainWindow saves prompt ID + sends to Terminal
4. **Git Commit:** Commit button -> async QProcess chain: init -> add -A -> commit (non-blocking)
5. **Settings:** SettingsDialog -> MainWindow applies to all components (language set before theme for single rehighlight)
6. **Git Status:** Timer (10s) -> async QProcess pipeline (includes --ignored) -> cache rebuild -> viewport update
7. **SSH Connect:** SshDialog -> MainWindow adds profile -> SshManager async mount -> profileConnected signal -> setup file browser + terminals
8. **SSH Reconnect:** Health timer -> async stat check -> connectionLost -> async fusermount + remount -> reconnected
9. **File Transfer:** Upload/Download via sshfs mount (rsync --progress or cp) -> transferProgress/transferFinished signals
10. **Tab Close:** maybeSaveTab() checks unsaved changes -> removeTab + widget->deleteLater() prevents memory leak
11. **Stop:** Stop button -> parse escape sequences -> sendText() to terminal
12. **Notifications:** Components call MainWindow::notify() -> NotificationPanel logs entry, updates unread badge (SSH events, transfers, git commits)
13. **Command Palette:** Ctrl+Shift+P -> CommandPalette.show() -> user selects command -> action callback executed
14. **Changes Monitor:** QFileSystemWatcher + scan timer (10s) -> changeDetected -> badge update on tab; user selects file -> git diff preview; Revert -> git checkout

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
