# Architecture

## Overview

Vibe Coder is a Qt6 C++ IDE-like application designed for AI-assisted development workflows. It combines a file browser, code editor, dual terminal emulators, SSH remote development, and prompt system into a single window with a custom title bar (CSD).

## Technology Stack

- **Language:** C++17
- **UI Framework:** Qt6 Widgets
- **Web Engine:** Qt6 WebEngineWidgets (markdown preview, mermaid rendering, PDF export)
- **PDF:** Qt6 PdfWidgets (QPdfDocument for page counting/rendering, QPdfWriter for output)
- **Terminal:** QTermWidget (libqtermwidget6)
- **Markdown:** libcmark (optional, loaded via dlopen) with regex fallback
- **Diagrams:** mermaid.js (bundled as Qt resource, ~3MB)
- **Math:** KaTeX (bundled JS + CSS + woff2 fonts)
- **Code Highlighting:** highlight.js 11.9.0 (bundled, 46+ languages)
- **SSH Filesystem:** sshfs / FUSE
- **Build System:** CMake 3.16+
- **Settings:** QSettings (INI-based, per-user)
- **External Themes:** Zed editor theme JSON format

## Application Structure

```
src/
├── main.cpp              Entry point, QApplication + MainWindow
├── mainwindow.h/cpp      Central orchestrator, menu, splitters, session, SSH coordination
├── filebrowser.h/cpp     Zed-style file tree with async git status, dual model (local/SSH)
├── codeeditor.h/cpp      Text editor with line numbers + syntax highlighting
├── terminalwidget.h/cpp  QTermWidget wrapper for embedded terminal
├── promptedit.h/cpp      Prompt input with configurable send key + save-and-send
├── settings.h/cpp        AppSettings struct, SettingsDialog (tabbed), global themes
├── project.h/cpp         .LLM/instructions.json project management + saved prompts
├── projectdialog.h/cpp   Create/Edit project dialog
├── sshdialog.h/cpp       SSH connection dialog with saved connections
├── sshmanager.h/cpp      Central SSH management: profiles, health, transfers, tunnels
├── sshtunneldialog.h/cpp Port forwarding management dialog
├── commandpalette.h/cpp  VS Code-style command palette (Ctrl+Shift+P)
├── diffviewer.h/cpp      Git diff viewer with syntax highlighting
├── changesmonitor.h/cpp  Real-time file change tracker with diff preview + revert
├── gitgraph.h/cpp        Git commit graph visualization with remote operations
├── notificationpanel.h/cpp  Log panel with Info/Warning/Error/Success levels
├── titlebar.h/cpp        Custom title bar (CSD) with minimize/maximize/close
├── themeddialog.h/cpp    Frameless dialog wrapper with themed title bar
├── zedthemeloader.h/cpp  Zed editor theme JSON parser and color mapper
├── markdownpreview.h/cpp Markdown preview with QWebEngineView + mermaid.js + highlight.js + KaTeX
├── resources/resources.qrc  Qt resource file (bundled JS/CSS/fonts)
├── resources/mermaid.min.js Mermaid.js library (~3MB, offline diagram rendering)
├── resources/highlight.min.js Highlight.js + 46 language modules (~270KB)
├── resources/hljs-dark.min.css VS2015 dark theme for code blocks
├── resources/hljs-light.min.css VS light theme for code blocks
├── resources/katex.min.js   KaTeX math rendering library
├── resources/katex.min.css  KaTeX stylesheet
├── resources/auto-render.min.js KaTeX auto-render extension
├── resources/fonts/         KaTeX woff2 fonts
```

## Component Responsibilities

### MainWindow
- Top-level layout: custom TitleBar + horizontal splitter (file browser | right panel)
- Frameless window (`Qt::FramelessWindowHint`) with custom CSD title bar (VS Code/Zed style)
- Native window drag via `startSystemMove()`, edge resize via `startSystemResize()` (Wayland compatible)
- Right panel: vertical splitter (editor splitter | bottom tab widget)
- Editor splitter: wraps main tab widget, supports horizontal/vertical split view for side-by-side editing
- Tab widget: AI-terminal (tab 0, non-closable) + editor tabs
- Bottom tab widget: Prompt tab + Terminal tab + Notifications tab + Diff Viewer tab + Changes tab + Git Graph tab
- Hamburger menu (top-right): Create/Edit Project, SSH Connect/Disconnect/Tunnels/Upload/Download, Split Horizontal/Vertical/Unsplit, Settings
- Command palette (Ctrl+Shift+P): fuzzy-searchable list of all registered commands (split, focus, theme switching, diff refresh)
- **Unified global theme system:** Built-in themes (Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord) + auto-discovered Zed editor themes. Single global stylesheet cascades to all widgets including title bar, menus, dialogs, tabs, splitters, status bar
- **Theme application order:** `applyGlobalTheme()` first (global QSS), then `applySettings()` (per-widget overrides) — ensures widget-specific styles take precedence
- Status bar: file info, SSH profile combo, transfer progress bar
- **Session persistence:** window geometry via `normalGeometry()`, splitter states, open files. Multi-monitor aware: finds target screen by geometry match, uses `setGeometry()` + deferred `showMaximized()`
- File watcher: auto-reload externally changed files (300ms debounce)
- Keyboard shortcuts: Ctrl+S save, Ctrl+F find, Ctrl+H find & replace, Ctrl+Shift+P command palette, Ctrl+M markdown preview, Ctrl+= / Ctrl+- context-dependent zoom (editor/prompt/browser/preview)
- **Stop button** — sends configurable stop sequence to terminal (default: `\x03`), supports escape sequences
- **Unsaved changes tracking** — `●` tab marker, Save/Discard/Cancel on tab close, Save All/Discard/Cancel on app close
- **Tab context menu** — Close, Close Others, Close All, Close to Right, Close to Left (all check unsaved changes)
- Tab close properly deletes CodeEditor widget (no memory leak)
- **Tab close icons** — dynamically generated PNG with theme-colored cross (✕), applied via stylesheet `image: url()` — automatically updates on theme change
- Async git commit via chained QProcess callbacks with `std::shared_ptr<QString>` (no leak on error)
- Notification system: centralized logging with unread badge count
- Changes monitor integration: tracks file changes per project, badge count on tab
- **Markdown preview** — multiple `MarkdownPreview` instances (one per .md file). Toggled as editor tab via Ctrl+M or 👁 tab button. Each preview has live updates connected to its source editor's `textChanged` signal. Source editor destruction auto-closes preview. Chromium pre-initialized via hidden 0x0 QWebEngineView at startup
- **Commit moved to Git tab** — commit button in GitGraph toolbar, shows themed dialog with editable commit message

### TitleBar
- Custom window title bar replacing system decorations (CSD — Client-Side Decorations)
- 32px height with title label + minimize/maximize/close buttons
- Close button with red hover (#e81123), other buttons with translucent hover
- Double-click toggles maximize, left-click drag via native `startSystemMove()`
- Colors inherited from global theme stylesheet

### ThemedDialog
- Utility for making QDialogs frameless with a themed title bar
- `ThemedDialog::apply(dialog, title)` wraps existing dialog layout with DialogTitleBar
- DialogTitleBar: same style as main TitleBar (close button, native drag)
- Applied to: SettingsDialog, ProjectDialog, SshDialog, SshTunnelDialog, Git Remotes dialog
- **ThemedMessageBox** — drop-in QMessageBox replacement with frameless themed dialog. Static methods: `question()`, `warning()`, `information()`. Used throughout the app instead of QMessageBox for consistent dark theme support
- Global stylesheet applied via `qApp->setStyleSheet()` ensures all dialogs (including message boxes) inherit theme colors

### FileBrowser
- Dual model: QFileSystemModel for local, QStandardItemModel for SSH (synchronous QDir reads over sshfs)
- QTreeView with custom FileItemDelegate for Zed editor-style rendering
- File type colors, directory arrows, git status colors (modified, untracked, added, ignored)
- **Async git status via 4-step QProcess pipeline** (non-blocking, 10s interval):
  1. `git rev-parse --is-inside-work-tree`
  2. `git rev-parse --show-toplevel`
  3. `git status --porcelain -unormal` (no --ignored flag)
  4. `git ls-files --others --ignored --exclude-standard --directory` + `git check-ignore --no-index --stdin` (recursive enumeration up to depth 6)
- **Instant git status updates:** QFileSystemWatcher monitors root path, git root, and .gitignore — triggers refresh with 300ms debounce (no waiting for 10s poll)
- Directory git status via precomputed cache (O(1) lookup) — covers modified, untracked, added (ignored NOT propagated to parents)
- Visibility filtering via `FileBrowserProxy` (QSortFilterProxyModel):
  - Gitignored files: visible / grayed / hidden (configurable in Settings > Visibility)
  - .git directory: visible / grayed / hidden (default: hidden)
  - Uses `beginFilterChange()/endFilterChange()` to preserve tree expansion state and scroll position
  - SSH model filters in `sshPopulateDir()`
- Context menu: New File, New Directory, Rename, Delete
- **Drag & drop** — `FileBrowserTreeView` subclass handles drop events, moves files/directories via `QFile::rename()`. Prevents moving into self or overwriting existing files
- **Simplified UI** — removed path text field, replaced with "Open Directory…" button showing current directory name
- SSH path translation: toRemotePath() maps mount paths to remote paths
- Theme-aware: accepts bg/fg color parameters from global theme, uses darker() for light themes / lighter() for dark themes
- Path traversal protection in file operations
- **QFileSystemWatcher limits** — max 4000 entries to stay within OS limits (~8192 on Linux)
- **Git output capping** — status/ls-files/check-ignore output truncated at 2MB
- **feedEntries limit** — max 5000 files fed to git check-ignore stdin
- **Git timer skip** — 10s timer skips refresh if previous git operation still running
- Git polling timer only starts after git repo confirmed, stops when no repo

### CodeEditor
- QPlainTextEdit subclass with LineNumberArea widget
- SyntaxHighlighter: C/C++, Python, JavaScript/TypeScript, Rust + search pattern overlay
- Dark/Light color scheme with matching syntax colors via `setEditorColorScheme(scheme, bg, fg)`
- QPalette-based coloring: Base, Text, AlternateBase, PlaceholderText derived from theme colors
- **Line number area:** explicit stylesheet override (`background-color` + `color`) to prevent global stylesheet from overriding QPalette colors
- Single rehighlight pass: setLanguage() builds rules, setEditorColorScheme() triggers rehighlight
- Configurable line number visibility
- **Find & Replace** (FindReplaceBar widget):
  - Ctrl+F opens find bar, Ctrl+H opens find & replace
  - Yellow highlight of all matches via syntax highlighter overlay
  - Yellow markers on vertical scrollbar via MarkerScrollBar (QScrollBar subclass)
  - Match counter ("3 of 12"), navigate with Enter/Shift+Enter or arrows
  - Replace one / Replace All with undo group support
  - Colors inherited from global stylesheet
- **Undo/Redo** — built-in QPlainTextEdit support (Ctrl+Z / Ctrl+Shift+Z)
- **Current line highlighting** — configurable via Settings > Editor
- **Unsaved changes tracking** — compares `toPlainText()` against saved content, shows `●` marker in tab title
- **Search debouncing** — 200ms delay before scanning document on keystroke
- **Large file mode** — files >1MB disable syntax highlighting and line wrapping for performance
- **O(1) match navigation** — `onFindNext()`/`onFindPrev()` use counter increment instead of O(n) document scan
- **Optimized line number painting** — reuses QString via `setNum()`, caches font metrics and area width

### TerminalWidget
- Thin wrapper around QTermWidget
- Shell started after widget is placed in layout (deferred startShellProgram)
- sendText() appends \r for proper Enter simulation
- Supports interactive apps (claude, mc, vim, etc.)
- Font and color scheme configurable via Settings
- Terminal color scheme mapped from global theme (Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight)
- Font applied via `setFamily()` + `setStyleHint(QFont::Monospace)`, reapplied after event loop via `QTimer::singleShot(0)` to override QTermWidget reset

### PromptEdit
- QPlainTextEdit subclass with custom keyPressEvent
- Configurable send key: Enter or Ctrl+Enter
- Shift modifier: send + save prompt
- Two signals: sendRequested() and saveAndSendRequested()
- Stylesheet-based coloring with font-family/font-size in stylesheet (prevents QFont/stylesheet cascade conflict)
- **Current line highlighting** — configurable via Settings > Prompt, auto-detects dark/light from palette

### ZedThemeLoader
- Scans for Zed editor theme JSON files in standard locations:
  - `~/.var/app/dev.zed.Zed/data/zed/extensions/installed/*/themes/*.json` (Flatpak)
  - `~/.local/share/zed/extensions/installed/*/themes/*.json` (native)
- Parses Zed theme JSON format (v0.2.0 schema): `themes[].style` object
- Color mapping from Zed to application:
  - `bgColor` ← `editor.background` or `background`
  - `textColor` ← `editor.foreground` or `text`
  - `altBg` ← `surface.background` or `panel.background` (fallback: computed)
  - `borderColor` ← `border` (fallback: computed)
  - `hoverBg` ← `element.hover` (fallback: computed)
  - `selectedBg` ← `element.selected` or first accent color (fallback: computed)
- Strips alpha channel from 8-char hex colors (#rrggbbaa → #rrggbb)
- Themes loaded once at startup, cached in `AppSettings::zedThemes`
- Zed themes prefixed with "Zed: " in UI to avoid name collisions

### SshManager
- Central SSH management replacing scattered SSH state
- **Profile management:** add/remove/connect/disconnect multiple simultaneous profiles
- **Async mount:** sshfs mount runs asynchronously via QProcess signals (no UI blocking)
- **Health check:** async sequential check of each profile's mount (stat with 3s timeout, 15s interval)
- **Auto-reconnect:** up to 3 attempts on connection loss, async fusermount + remount
- **File transfer:** upload/download via sshfs mount point (rsync --progress or cp)
- **Port forwarding:** SSH tunnels (-L/-R) with SSH_ASKPASS for password auth
- **Security:** allowlist-based input validation (`^[a-zA-Z0-9._@-]+$`, max 253 chars), identity file existence check, password never in process args, memfd_create for password storage (Linux)
- **Cached rsync detection:** async `which rsync` at construction, result cached (non-blocking)
- **Async unmount:** `doUnmount()` uses chained async QProcess (no `waitForFinished`), optional completion callback

### SshTunnelDialog
- QDialog for creating/viewing SSH port forwards
- Form: name, direction (Local -L / Remote -R), local port, remote host, remote port
- QTableWidget showing active tunnels with Remove buttons
- Auto-refreshes on tunnel start/stop signals
- Themed via ThemedDialog (frameless with custom title bar)

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
- Built-in commands: split/unsplit, focus panels, theme switching (built-in + Zed themes), diff refresh
- Colors inherited from global stylesheet

### DiffViewer
- QPlainTextEdit with DiffHighlighter (added/removed/hunk header coloring)
- Mode combo for switching diff views (e.g. staged vs unstaged)
- Refresh button triggers `git diff` on current working directory
- Configurable font via Settings (diff section)
- Embeds as a tab in the bottom tab widget
- Output truncated at 512KB to prevent GUI freeze on large diffs

### ChangesMonitor
- Real-time file change tracker using QFileSystemWatcher + periodic scan (10s, only when visible)
- Split layout: file list (left, max 300px) + diff preview (right) with DiffBlockHighlighter
- Toolbar: info label, Open, Revert, Refresh, Clear buttons
- Watches up to 2000 files and 500 directories (skips .git, .LLM, node_modules, __pycache__, build, .cache, target, dist, vendor)
- Scan limited to 5000 files to prevent CPU spikes on large projects
- Deduplicates changes (same file replaces previous entry, newest first)
- 1-second debounce to avoid duplicate signals
- Revert: runs `git checkout -- <file>` asynchronously, marks entry as reverted
- Open: emits `openFileRequested` signal to open file in editor
- Diff preview: runs `git diff` for tracked files, shows content for new files
- Re-adds watched files after signal (QFileSystemWatcher removes on some platforms)
- Badge count on bottom tab: "Changes (N)"
- Configurable font via Settings (changes section)

### MarkdownPreview
- QWebEngineView-based live markdown preview, opened as editor tab via Ctrl+M or 👁 tab button
- **Multiple instances** — each `.md` file can have its own independent preview tab. Source editor tracked via `QObject::property("sourceEditor")`. Closing source editor auto-closes preview via `QObject::destroyed` signal
- **Markdown conversion:** libcmark loaded via `dlopen()` at runtime (tries libcmark.so, .so.0, etc.). Falls back to built-in regex converter supporting: headings, bold/italic/strikethrough, code blocks with language class, tables, lists, blockquotes, horizontal rules, links, images. If `dlsym` fails, handle is properly `dlclose`d (no leak)
- **Syntax highlighting:** highlight.js 11.9.0 bundled with 46+ language modules (core + x86asm, armasm, avrasm, mipsasm, glsl, vhdl, verilog, cmake, dockerfile, makefile, kotlin, swift, dart, julia, matlab, fortran, haskell, erlang, elixir, clojure, scala, lua, latex, protobuf, graphql, wasm, arduino, powershell, and more). VS2015 (dark) and VS (light) themes. `hljs.highlightElement()` called on every content update for `<code>` blocks with language class
- **KaTeX math** — bundled KaTeX JS + CSS + woff2 fonts. Supports `$$...$$` (display), `$...$` (inline), `\(...\)`, `\[...\]` delimiters. Math expressions protected from markdown processing before conversion, restored as HTML-escaped text for KaTeX to render
- **Mermaid diagrams:** mermaid.js (~3MB) bundled as Qt resource, extracted to temp dir at startup. Loaded via `<script src="file://...">` to avoid QWebEngineView's 2MB `setHtml()` limit
- **Rendering architecture:** base HTML page (CSS + mermaid.js + highlight.js + KaTeX) loaded once from temp file. Content updates via `runJavaScript("updateBody('...')")` — no page reload, preserves scroll position. Update pipeline: innerHTML → highlight.js → mermaid.run() → KaTeX renderMathInElement()
- **Dark/light theme:** full CSS theming with theme-appropriate mermaid theme (`dark`/`default`) and matching highlight.js theme
- **Chromium pre-initialization** — hidden 0x0 QWebEngineView loads `about:blank` at startup, forcing Chromium engine init without visible flicker
- **Zoom** — `zoomIn()`/`zoomOut()` methods adjust `QWebEngineView::setZoomFactor()` (0.25x–5.0x). Context-dependent: Ctrl+=/- only affects the focused component
- **500ms debounce timer** prevents excessive re-renders during fast typing
- **Offline operation:** all resources local, `LocalContentCanAccessRemoteUrls` disabled
- **Security hardening** — `PluginsEnabled`, `PdfViewerEnabled`, `NavigateOnDropEnabled` all disabled
- **PDF Export** — two-pass rendering pipeline:
  1. `injectPrintCss()` adds `@media print` CSS for text wrapping (`pre-wrap`, `word-break`, `table-layout: fixed`)
  2. `printToPdf()` with callback generates base PDF as QByteArray
  3. `postProcessPdf()` opens PDF via `QPdfDocument`, renders each page to QImage at 300 DPI
  4. `QPdfWriter` + `QPainter` creates final PDF: draws page image, white margin overlays (3px overlap to cover edge artifacts), optional page border, and centered page numbers
  - Configurable: left/right margins, portrait/landscape, page numbering (none/page/page+total), page border
  - Print CSS cleanup via `removePrintCss()` after export completes

### GitGraph
- Visual commit history graph as bottom tab ("Git")
- Custom `GitGraphView` widget with QPainter-drawn commit graph:
  - Colored lane lines (8 rotating colors), curved bezier merge lines
  - Filled circles (normal), hollow circles (merge), diamonds (remote-only commits)
  - Remote-only commits: dashed lines, dimmed text
- Ref labels: green (local branch), orange (remote branch), yellow (tag), blue (HEAD)
- Commit metadata: short hash, subject (elided), author, relative date
- **Branch selector** combo: local branches, separator, remote branches (`git branch -a`), or `--all--`
- **Remote operations:** Fetch (`git fetch --all --prune`), Pull (`git pull`), Push (`git push`) buttons with loading state
- **Upstream tracking info:** label showing `branch -> upstream [ahead N, behind M]` with color coding (green=ahead, yellow=behind, red=both)
- **Remotes dialog:** view/add/edit/remove multiple git remotes (name + URL). Supports `git remote add/set-url/rename/remove`
- **Commit button** in toolbar: shows themed dialog with editable commit message (default: timestamp), runs `git add .` + `git commit -m "message"`, auto-refreshes graph after commit
- **User info button** in toolbar: displays `git config --global user.name <email>`. Click opens edit dialog to change name/email (async QProcess, non-blocking)
- Auto-refreshes on directory change and tab activation
- Configurable font and colors from Settings (uses diff font settings)
- `outputMessage` signal for notification integration

### NotificationPanel
- Timestamped log entries with 4 severity levels: Info, Warning, Error, Success
- QListWidget with color-coded entries and level icons
- Unread count with `unreadChanged` signal for badge updates
- Clear button to reset the log

### Settings
- AppSettings struct with load/save via QSettings("vibe-coder", "vibe-coder")
- **Global theme:** unified system — one theme controls all UI components
  - Built-in: Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord
  - External: auto-discovered Zed editor themes (prefixed "Zed: ")
  - `applyThemeDefaults()` derives all component colors from globalTheme (editor/browser/terminal color schemes, bgColor, textColor)
- **Theme cascade:** `applyGlobalTheme()` sets comprehensive QSS stylesheet covering QWidget, QLabel, QPlainTextEdit, QTextEdit, QLineEdit, QSpinBox, QComboBox, QCheckBox, QPushButton, QToolButton, QListWidget, QMenu, QDialog, QGroupBox, QScrollBar, QProgressBar, QTabWidget, QTabBar, QFontComboBox, QDialogButtonBox
- **Live theme switching:** theme changes apply immediately without restart
- Tabbed SettingsDialog: Global Theme (top, includes separator + Zed themes), Terminal (theme override + font), Editor, File Browser, Prompt, Diff Viewer, Changes Monitor, Visibility, PDF tabs
- **Terminal theme override:** "Auto" (follows global theme) or explicit: Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight
- SettingsDialog preserves zedThemes list through `result()` method
- **Font size clamping:** all font sizes clamped to 6–72 range after loading to prevent invalid values from corrupted settings

## Data Flow

1. **File Opening:** FileBrowser emits `fileOpened` -> MainWindow creates CodeEditor tab, adds to FileWatcher
2. **Prompt Sending:** PromptEdit emits `sendRequested` -> MainWindow logs to Project + sends to Terminal
3. **Save+Send:** PromptEdit emits `saveAndSendRequested` -> MainWindow saves prompt ID + sends to Terminal
4. **Git Commit:** Commit button (Git tab) -> themed dialog for message -> async QProcess chain: init -> add . -> commit -m "message" -> refresh git graph (non-blocking, shared_ptr for output)
5. **Settings:** SettingsDialog -> MainWindow applies `applyGlobalTheme()` first, then `applySettings()` to all components (language set before theme for single rehighlight)
6. **Git Status:** Timer (10s) + QFileSystemWatcher (instant) -> async QProcess pipeline (git status + ls-files + check-ignore) -> cache rebuild -> viewport update
7. **SSH Connect:** SshDialog -> MainWindow adds profile -> SshManager async mount -> profileConnected signal -> setup file browser + terminals
8. **SSH Reconnect:** Health timer -> async stat check -> connectionLost -> async fusermount + remount -> reconnected
9. **File Transfer:** Upload/Download via sshfs mount (rsync --progress or cp) -> transferProgress/transferFinished signals
10. **Tab Close:** maybeSaveTab() checks unsaved changes -> removeTab + widget->deleteLater() prevents memory leak
11. **Stop:** Stop button -> parse escape sequences -> sendText() to terminal
12. **Notifications:** Components call MainWindow::notify() -> NotificationPanel logs entry, updates unread badge (SSH events, transfers, git commits)
13. **Command Palette:** Ctrl+Shift+P -> CommandPalette.show() -> user selects command -> action callback executed
14. **Changes Monitor:** QFileSystemWatcher + scan timer (10s) -> changeDetected -> badge update on tab; user selects file -> git diff preview; Revert -> git checkout
15. **Theme Switch:** Settings/CommandPalette -> applyGlobalTheme() (global QSS) -> applySettings() (per-widget overrides) -> force repaint all children
16. **Git Graph:** Tab activation -> loadBranches + loadLog + loadTrackingInfo + loadRemotes + loadUserInfo -> GitGraphView.setCommits() -> QPainter render
17. **Remote Operations:** Fetch/Pull/Push buttons -> async QProcess -> outputMessage signal -> NotificationPanel; Remotes dialog -> git remote add/set-url/rename/remove
18. **Markdown Preview:** Ctrl+M on .md file (or 👁 tab button) -> create new MarkdownPreview instance -> add as tab -> connect editor.textChanged -> 500ms debounce -> markdownToHtml (cmark or regex) -> runJavaScript("updateBody(html)") -> hljs.highlightElement() -> mermaid.run() if needed -> renderMathInElement() for KaTeX
19. **PDF Export:** Command Palette "Export Preview to PDF" -> QFileDialog for path -> injectPrintCss -> printToPdf (QByteArray callback) -> QPdfDocument count pages -> QPdfWriter+QPainter render each page (image + white margin overlays + optional border + optional page number) -> removePrintCss

## Session Persistence

Stored in QSettings on application close:
- Window geometry via `normalGeometry()` (not `pos()` — handles maximized state correctly)
- Target screen identification by geometry match for multi-monitor setups
- Main splitter and right splitter positions
- File browser root path
- List of open file paths
- Active tab index

## Security Model

- SSH passwords: sent via PTY (echo off) for terminals, password_stdin for sshfs, SSH_ASKPASS for tunnels
- Passwords never persisted to disk (QSettings stores connections without password)
- Passwords never appear in process argument lists (no sshpass -p)
- On Linux, passwords written to in-memory file descriptor via `memfd_create(MFD_CLOEXEC)` — never touches filesystem. Accessed via `/proc/self/fd/N`. Falls back to `QTemporaryFile` on non-Linux
- SSH user/host validated via allowlist regex (`^[a-zA-Z0-9._@-]+$`, max 253 chars)
- SSH identity file validated for existence before use
- File names sanitized against newline/null injection in git operations
- File/directory creation validates against null bytes and newlines
- Git commit auto-adds .env, *.pem, *.key, credentials.json to .gitignore
- Path traversal protection in file browser operations
- StrictHostKeyChecking=accept-new for SSH connections
- SSH tunnel remoteHost validated with `^[a-zA-Z0-9._-]+$` regex
- SSH tunnel direction must be exactly "local" or "remote"
- SSH mount point includes random suffix (QRandomGenerator) to prevent TOCTOU
- Terminal `cd` commands quote paths with double quotes (handles spaces and special chars)
- WebEngine `LocalContentCanAccessRemoteUrls` disabled in markdown preview
- WebEngine `PluginsEnabled`, `PdfViewerEnabled`, `NavigateOnDropEnabled` disabled
- QFileSystemWatcher `addPaths()` return value checked for platform limit failures
- Bundled JS assets (mermaid.js, highlight.js, KaTeX) integrity-checked on startup — on-disk copies compared against Qt resource originals, overwritten if tampered
