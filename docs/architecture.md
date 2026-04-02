# Architecture

## Overview

Vibe Coder is a Qt6 C++ IDE-like application designed for AI-assisted development workflows. It combines a file browser, code editor, dual terminal emulators, SSH remote development, and prompt system into a single window with a custom title bar (CSD).

## Technology Stack

- **Language:** C++17
- **UI Framework:** Qt6 Widgets
- **Web Engine:** Qt6 WebEngineWidgets (markdown preview, mermaid rendering, PDF export)
- **PDF:** Qt6 PdfWidgets (QPdfDocument for page counting/rendering, QPdfWriter for output)
- **Terminal:** QTermWidget (libqtermwidget6)
- **Markdown:** cmark-gfm (GitHub Flavored Markdown, statically linked via CMake FetchContent)
- **Diagrams:** mermaid.js (bundled as Qt resource, ~3MB)
- **Math:** KaTeX (bundled JS + CSS + woff2 fonts)
- **Code Highlighting:** highlight.js 11.9.0 (bundled, 46+ languages)
- **SSH Filesystem:** sshfs / FUSE
- **Build System:** CMake 3.16+
- **Settings:** QSettings (INI-based, per-user)
- **External Themes:** `~/.config/vibe-coder/themes/` — native JSON, Zed, and VS Code formats

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
├── zedthemeloader.h/cpp  External theme loader (native/Zed/VS Code JSON formats)
Note: cmark-gfm is fetched and statically linked via CMake FetchContent (no source files in tree)
├── markdownpreview.h/cpp Markdown preview with QWebEngineView + mermaid.js + highlight.js + KaTeX
├── fileopener.h/cpp      Fuzzy file opener popup (Ctrl+P)
├── workspacesearch.h/cpp Full-text workspace search (Ctrl+Shift+F)
├── gitblame.h/cpp        Git blame viewer with gutter annotations
├── resources/vibe-coder.svg Application icon (SVG, embedded via QRC)
├── resources/resources.qrc  Qt resource file (bundled JS/CSS/fonts/icon)
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
- Bottom tab widget: Prompt tab + Terminal tab + Notifications tab + Diff Viewer tab + Changes tab + Git Graph tab + Search tab + Blame tab
- Hamburger menu (top-right): Create/Edit Project, SSH Connect/Disconnect/Tunnels/Upload/Download, Split Horizontal/Vertical/Unsplit, Settings
- Command palette (Ctrl+Shift+P): fuzzy-searchable list of all registered commands (split, focus, theme switching, diff refresh)
- **Unified global theme system:** All themes loaded from `~/.config/vibe-coder/themes/` (native JSON, Zed, VS Code formats). Ships with 8 themes. Single global stylesheet cascades to all widgets including title bar, menus, dialogs, tabs, splitters, status bar
- **Theme application order:** `applyGlobalTheme()` first (global QSS with GUI-blended text color), then `applySettings()` (per-widget overrides with per-component intensity) — ensures widget-specific styles take precedence. Per-widget stylesheets use `objectName` + ID selectors to override global type selectors
- **Font intensity:** `blendIntensity()` helper blends text color towards background color by intensity factor. Applied in: global stylesheet (GUI), `setEditorColorScheme()` + `SyntaxHighlighter::setIntensity()` (editor), `FileItemDelegate::blendColor()` (file browser), per-widget stylesheets with ID selectors (prompt, diff, changes), `setTerminalOpacity()` (terminal)
- **Tab maximize:** double-click on tab bar toggles full-window mode by hiding/showing file browser and the opposite panel, saving/restoring splitter sizes
- Status bar: file info, SSH profile combo, transfer progress bar
- **Session persistence:** window geometry via `normalGeometry()`, splitter states, open files. Multi-monitor aware: finds target screen by geometry match, uses `setGeometry()`. Maximize state restored via `m_restoreMaximized` flag checked in `main.cpp` (single show, no double-display)
- **Terminal directory policy:** terminals change directory only when a folder is opened via the Open Directory dialog (`rootPathOpenedByDialog` signal). Tree navigation (double-click) and file opening do not affect terminal working directory
- File watcher: auto-reload externally changed files (300ms debounce)
- Keyboard shortcuts: Ctrl+S save, Ctrl+F find, Ctrl+H find & replace, Ctrl+P fuzzy file opener, Ctrl+Shift+P command palette, Ctrl+Shift+F workspace search, Ctrl+M markdown preview, Ctrl+= / Ctrl+- context-dependent zoom (editor/prompt/browser/preview)
- **Stop button** — sends configurable stop sequence to terminal (default: `\x03`), supports escape sequences
- **Unsaved changes tracking** — `●` tab marker, Save/Discard/Cancel on tab close, Save All/Discard/Cancel on app close
- **Tab context menu** — Close, Close Others, Close All, Close to Right, Close to Left (all check unsaved changes)
- Tab close properly deletes CodeEditor widget (no memory leak)
- **Tab close icons** — dynamically generated PNG with theme-colored cross (✕), applied via stylesheet `image: url()` — automatically updates on theme change
- **Active tab accent border** — `QTabBar::tab:selected` styled with 2px bottom border in theme accent color for clear tab identification across all themes
- **Minimal splitter handles** — `QSplitter::handle` width/height set to 1px via global stylesheet
- Async git commit via chained QProcess callbacks with `std::shared_ptr<QString>` (no leak on error)
- Notification system: centralized logging with unread badge count
- Changes monitor integration: tracks file changes per project, badge count on tab
- **Markdown preview** — multiple `MarkdownPreview` instances (one per .md file). Toggled as editor tab via Ctrl+M or 👁 tab button. Each preview has live updates connected to its source editor's `textChanged` signal. Source editor destruction auto-closes preview. Chromium pre-initialized via hidden `MarkdownPreview` instance created at startup (`m_previewWarmup`); first user preview reuses it
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
- **Single-command git status** (non-blocking, 10s adaptive polling):
  - First call: `git rev-parse --show-toplevel` to discover and **cache** git root (not repeated on subsequent refreshes)
  - Subsequent calls: `git --no-optional-locks status --porcelain -unormal --ignored` — single process, returns both file status AND ignored files (`!!` entries). No shell wrapper, no `ls-files`, no `check-ignore`
  - `--no-optional-locks` prevents blocking when another git process is running
- **SSH git operations run on remote server** — when SSH is active, git commands are executed via `ssh user@host "cd /path && git ..."` instead of on the slow sshfs FUSE mount. Uses SSH ControlMaster multiplexing (`ControlMaster=auto`, `ControlPersist=120`) to reuse a single SSH connection for all git refreshes (eliminates repeated SSH handshake overhead). SSH debounce is 1500ms (vs 150ms local)
- **Instant git status updates:** QFileSystemWatcher monitors `.git/` directory (catches index, HEAD, refs changes all at once) and `.gitignore`. Triggers refresh with 150ms debounce (no waiting for 10s poll). Watches are automatically re-added after each git refresh to handle atomic writes (editors use rename-over, which causes inotify to drop the watch)
- Directory git status via precomputed cache (O(1) lookup) — covers modified, untracked, added (ignored NOT propagated to parents)
- Visibility filtering via `FileBrowserProxy` (QSortFilterProxyModel):
  - Gitignored files: visible / grayed / hidden (configurable in Settings > Visibility)
  - .git directory: visible / grayed / hidden (default: hidden)
  - Uses `beginFilterChange()/endFilterChange()` to preserve tree expansion state and scroll position
  - SSH model filters in `sshPopulateDir()`
- Context menu: New File, New Directory, Rename, Delete
- **Drag & drop** — `FileBrowserTreeView` subclass handles drop events, moves files/directories via `QFile::rename()`. Prevents moving into self or overwriting existing files
- **Simplified UI** — removed path text field, replaced with "Open Directory…" button showing current directory name + animated refresh button (custom QPainter-drawn rotating arrow, 15°/30ms spin on click)
- SSH path translation: toRemotePath() maps mount paths to remote paths
- Theme-aware: accepts bg/fg/hover/selected/lineHighlight color parameters from active theme. Selected file uses lineHighlight color (same as editor current line). Auto-highlights file corresponding to active editor tab
- Path traversal protection in file operations
- **QFileSystemWatcher limits** — max 4000 entries to stay within OS limits (~8192 on Linux)
- **Git output capping** — status output truncated at 2MB
- **Git timer skip** — 10s timer skips refresh if previous git operation still running
- Git polling timer only starts after git repo confirmed, stops when no repo
- **Git root caching** — `rev-parse --show-toplevel` called once per root path change, cached in `m_gitRoot`. Cleared on `setRootPath()` to force rediscovery

### CodeEditor
- QPlainTextEdit subclass with LineNumberArea widget and SpacedDocumentLayout for configurable line spacing
- SyntaxHighlighter: C/C++, Python, JavaScript/TypeScript, Rust + search pattern overlay. Multi-line comment handling uses block state tracking with correct search offset (0 when continuing from previous block, +2 when starting from new `/*`)
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
- **Current line highlighting** — configurable via Settings > Editor, color derived from active theme (`lineHighlight` field or auto-computed)
- **Unsaved changes tracking** — compares `toPlainText()` against saved content, shows `●` marker in tab title
- **Search debouncing** — 200ms delay before scanning document on keystroke
- **Large file mode** — files >1MB disable syntax highlighting and line wrapping for performance
- **O(1) match navigation** — `onFindNext()`/`onFindPrev()` use counter increment instead of O(n) document scan
- **Optimized line number painting** — reuses QString via `setNum()`, caches font metrics and area width
- **Bracket matching** — highlights matching `()`, `{}`, `[]` pairs at cursor position via extra selections. Searches forward for opening brackets, backward for closing. Depth-tracked for nested brackets
- **Auto-close brackets** — typing `(`, `{`, `[`, `"`, `'` inserts closing pair and positions cursor between them. Typing a closing bracket skips over existing one. Backspace between a pair deletes both characters

### TerminalWidget
- Thin wrapper around QTermWidget
- Shell start deferred via `QTimer::singleShot(0)` — constructor doesn't block, shell launches after first event loop cycle
- sendText() appends \r for proper Enter simulation
- Supports interactive apps (claude, mc, vim, etc.)
- Font and color scheme configurable via Settings
- Terminal color scheme mapped from global theme (Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight)
- Font applied via `setFamily()` + `setStyleHint(QFont::Monospace)`, reapplied after event loop via `QTimer::singleShot(0)` to override QTermWidget reset
- **Activity monitoring** — AI-terminal content changes detected via periodic screen snapshot comparison (`QWidget::grab()` + `QCryptographicHash::Md5`, 400ms interval). Works even when terminal tab is hidden. Drives animated spinner indicator in prompt area

### PromptEdit
- QPlainTextEdit subclass with custom keyPressEvent
- Configurable send key: Enter or Ctrl+Enter
- Shift modifier: send + save prompt
- Two signals: sendRequested() and saveAndSendRequested()
- Stylesheet-based coloring with font-family/font-size in stylesheet (prevents QFont/stylesheet cascade conflict)
- **Current line highlighting** — configurable via Settings > Prompt, auto-detects dark/light from palette
- **AI-terminal button** — quick switch to AI-terminal tab (index 0)
- **Activity indicator** — animated label (`| / - \` at 200ms) driven by terminal content change detection. `●` when idle, spinning when content changes. Idle timeout: 1.5s

### ExternalThemeLoader
- Loads all themes from `~/.config/vibe-coder/themes/` (recursive JSON scan)
- Auto-detects format by JSON structure:
  - **Native format** — flat JSON with `background`, `foreground`, `altBackground`, `border`, `hover`, `selected`, `lineHighlight`, `terminalScheme`, `appearance`
  - **Zed format** — `themes[]` array with `style` object (`editor.background`, `editor.foreground`, `surface.background`, `border`, `element.hover`, `element.selected`, `editor.active_line.background`)
  - **VS Code format** — `colors` object (`editor.background`, `editor.foreground`, `sideBar.background`, `editor.lineHighlightBackground`, `list.hoverBackground`, `list.activeSelectionBackground`)
- 7 color values per theme: bgColor, textColor, altBg, borderColor, hoverBg, selectedBg, lineHighlight
- Missing colors auto-derived from background using `lighter()`/`darker()`
- Optional `terminalScheme` field maps to terminal color scheme (Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight)
- Strips alpha channel from 8-char hex colors (#rrggbbaa → #rrggbb)
- Themes loaded once at startup, cached in `AppSettings::externalThemes`
- Ships with 8 themes in `themes/` directory (Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord)

### SshManager
- Central SSH management replacing scattered SSH state
- **Profile management:** add/remove/connect/disconnect multiple simultaneous profiles
- **Async mount:** sshfs mount runs asynchronously via QProcess signals. Password delivery waits for `started` signal. FUSE caching enabled for responsive file browsing over high-latency networks (Tailscale, VPN)
- **SSH git via remote execution:** git operations (status, log, branch, etc.) run on the remote server via `ssh user@host` instead of on the sshfs mount. SSH ControlMaster multiplexing reuses a single connection (`ControlPersist=120s`)
- **Health check:** async sequential check of each profile's mount (stat with 3s timeout, 15s interval)
- **Auto-reconnect:** up to 3 attempts on connection loss, async fusermount + remount
- **File transfer:** upload/download via sshfs mount point (rsync --progress or cp)
- **Port forwarding:** SSH tunnels (-L/-R) with SSH_ASKPASS for password auth
- **Security:** allowlist-based input validation (`^[a-zA-Z0-9._@-]+$`, max 253 chars), identity file existence check, password never in process args, memfd_create for password storage (Linux)
- **Cached rsync detection:** async `which rsync` at construction, result cached (non-blocking)
- **Async unmount:** `doUnmount()` uses chained async QProcess (no `waitForFinished`), optional completion callback
- **Signal ordering:** `disconnectProfile()` updates all internal state (active index, health check) before emitting `profileDisconnected` — ensures handlers see consistent state

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
- Built-in commands: split/unsplit, focus panels, theme switching (all themes from `~/.config/vibe-coder/themes/`), diff refresh
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
- **Markdown conversion:** cmark-gfm (GitHub Flavored Markdown) statically linked. GFM extensions enabled: table, strikethrough, autolink, tagfilter. No runtime library dependency, no regex fallback needed
- **Syntax highlighting:** highlight.js 11.9.0 bundled with 46+ language modules. VS2015 (dark) and VS (light) themes. `hljs.highlightAll()` called on every page load
- **KaTeX math** — bundled KaTeX JS + CSS + woff2 fonts. Supports `$$...$$` (display) and `$...$` (inline) delimiters. Math detection operates on AST text nodes only — code spans and code blocks are inherently excluded by the parser, so `$` inside code never triggers math rendering. Math expressions injected as `CMARK_NODE_HTML_INLINE` nodes with KaTeX span wrappers. `katex.render()` called per element in browser
- **Mermaid diagrams:** mermaid.js (~3MB) bundled as Qt resource, extracted to temp dir at startup. Mermaid blocks rendered as `<div class="mermaid">` with `suppressErrors` and error styling fallback
- **Rendering architecture (in-memory):** each render generates a complete HTML page (CSS + JS + converted body), loaded via `setHtml()` with temp dir base URL for local file:// resource access. Update pipeline: page load → hljs.highlightAll() → mermaid.run() → katex.render() per span
- **Chromium pre-initialization:** a hidden `MarkdownPreview` instance (`m_previewWarmup`) is created at MainWindow startup to fully initialize the Chromium subprocess. First user preview reuses this instance, eliminating window resize/flicker on first open
- **AST-based conversion pipeline:** (1) cmark-gfm parses markdown into AST with GFM extensions, (2) `injectMathSpans()` walks AST text nodes, finds `$...$`/`$$...$$` patterns, replaces with `CMARK_NODE_HTML_INLINE` KaTeX spans, (3) `cmark_render_html()` produces final HTML. Code nodes are never visited, eliminating all code-vs-math conflicts
- **Dark/light theme:** full CSS theming with theme-appropriate mermaid theme (`dark`/`default`) and matching highlight.js theme
- **Zoom** — `zoomIn()`/`zoomOut()` methods adjust `QWebEngineView::setZoomFactor()` (0.25x–5.0x). Context-dependent: Ctrl+=/- only affects the focused component
- **500ms debounce timer** prevents excessive re-renders during fast typing
- **Offline operation:** all resources local, `LocalContentCanAccessRemoteUrls` disabled
- **Security hardening** — `PluginsEnabled`, `PdfViewerEnabled`, `NavigateOnDropEnabled` all disabled
- **PDF Export** — two-pass rendering pipeline:
  1. `@media print` CSS embedded directly in page (pre-wrap, word-break, overflow-visible for code blocks)
  2. `printToPdf()` with callback generates base PDF as QByteArray
  3. `postProcessPdf()` opens PDF via `QPdfDocument`, renders each page to QImage at 300 DPI
  4. `QPdfWriter` + `QPainter` creates final PDF: draws page image, white margin overlays (3px overlap to cover edge artifacts), optional page border, and centered page numbers
  - Configurable: left/right margins, portrait/landscape, page numbering (none/page/page+total), page border

### GitGraph
- Visual commit history graph as bottom tab ("Git")
- Custom `GitGraphView` widget with QPainter-drawn commit graph:
  - Colored lane lines (8 rotating colors), curved bezier merge lines
  - Filled circles (normal), hollow circles (merge), diamonds (remote-only commits)
  - Remote-only commits: dashed lines, dimmed text
- Ref labels: green (local branch), orange (remote branch), yellow (tag), blue (HEAD)
- Commit metadata: short hash, subject (elided), author, relative date
- **Single combined refresh** — all git data (branches, tracking, remotes, user, local hashes, log) loaded in one `sh -c` command with marker-separated sections. Replaces 5-7 sequential QProcess spawns
- **O(n) commit reachability** — remote-only commit detection uses hash-map indexed BFS (stack-based) instead of O(n²) repeated full-array scans
- **SSH-aware** — when SSH is active, git commands run directly on the remote server via `ssh user@host "cd /path && ..."` with ControlMaster multiplexing. Avoids running git on slow sshfs FUSE mount
- **Branch selector** combo: local branches, separator, remote branches (`git branch -a`), or `--all--`
- **Remote operations:** Fetch (`git fetch --all --prune`), Pull (`git pull`), Push (`git push`) buttons with loading state
- **Upstream tracking info:** label showing `branch -> upstream [ahead N, behind M]` with color coding (green=ahead, yellow=behind, red=both)
- **Remotes dialog:** view/add/edit/remove multiple git remotes (name + URL). Supports `git remote add/set-url/rename/remove`
- **Commit button** in toolbar: shows themed dialog with editable commit message (default: timestamp), runs `git add .` + `git commit -m "message"`, auto-refreshes graph after commit
- **User info button** in toolbar: displays `git config --global user.name <email>`. Click opens edit dialog to change name/email (async QProcess, non-blocking)
- Auto-refreshes on directory change and tab activation
- Configurable font and colors from Settings (uses diff font settings)
- `outputMessage` signal for notification integration

### FileOpener
- Popup widget (Ctrl+P) for fuzzy file search and quick opening
- Scans up to 50,000 files in project directory, skips `.git`, `node_modules`, `__pycache__`, `.LLM`, `build`, `dist`, `.cache`, `venv`, `target`, `cmake-build-*`
- Fuzzy scoring: substring matches score higher, filename-only matches get bonus, shorter paths preferred
- Arrow keys navigate, Enter opens, Escape dismisses
- File list cached per root path, rescan on directory change

### WorkspaceSearch
- Full-text search across project files (Ctrl+Shift+F), implemented as bottom tab ("Search")
- Uses system `grep` with `--max-count=500` for performance
- Excludes `.git`, `node_modules`, `__pycache__`, `build`, `dist`, `.cache`, `target`, `.LLM`
- Options: case sensitive (`-i`), regex (`-E`) / fixed string (`-F`)
- Split view: result list (left) + context preview (right, 7 lines around match)
- Single click shows context, double click opens file at line in editor
- `fileRequested` signal carries file path + line number; MainWindow opens file and jumps to line

### GitBlame
- Git blame viewer as bottom tab ("Blame"), auto-triggers when tab is activated with a file open
- Runs `git blame --porcelain` and parses output (40-char hex hash detection, author, author-time, tab-prefixed code lines)
- `BlameView` (QPlainTextEdit subclass) with `BlameGutter` widget (same pattern as CodeEditor's LineNumberArea)
- Gutter shows: commit hash (8 chars), author (16 chars), date — first line of each commit group shows full annotation, subsequent lines show dimmed hash only
- Alternating background colors per commit for visual grouping
- Refresh button to re-blame current file

### NotificationPanel
- Timestamped log entries with 4 severity levels: Info, Warning, Error, Success
- QListWidget with color-coded entries and level icons
- Unread count with `unreadChanged` signal for badge updates
- Clear button to reset the log

### Settings
- AppSettings struct with load/save via QSettings("vibe-coder", "vibe-coder")
- **Global theme:** unified system — one theme controls all UI components
  - All themes loaded from `~/.config/vibe-coder/themes/` (native JSON, Zed, VS Code formats)
  - `applyThemeDefaults()` derives all component colors from selected theme (editor/browser/terminal color schemes, bgColor, textColor, lineHighlight)
- **Widget style:** configurable Qt widget style via `QStyleFactory::create()`. Available styles auto-detected from installed Qt6 plugins (Fusion, Windows always built-in; Breeze, Adwaita, Oxygen, Kvantum available via system packages). Applied at startup in `main.cpp` before widget creation, and live-switchable via `qApp->setStyle()` in `applySettings()`
- **Theme cascade:** `applyGlobalTheme()` sets comprehensive QSS stylesheet with modern rounded design (border-radius: 6px on inputs/buttons/combos, 8px on menus/group boxes). Covers QWidget, QLabel, QPlainTextEdit, QTextEdit, QLineEdit, QSpinBox, QComboBox (with custom drop-down), QCheckBox (custom indicator), QPushButton (hover/pressed/disabled states), QToolButton, QListWidget (rounded items), QTableWidget, QHeaderView, QMenu (rounded with padded items), QDialog, QGroupBox, QScrollBar (thin, rounded, hover), QProgressBar, QTabWidget (flat borderless pane), QTabBar (accent underline), QFontComboBox, QDialogButtonBox, QToolTip, QScrollArea
- **Live theme switching:** theme changes apply immediately without restart
- Tabbed SettingsDialog: Global Theme (top, populated from `~/.config/vibe-coder/themes/`), GUI (widget style + font), Terminal (theme override + font), Editor, File Browser, Prompt, Diff Viewer, Changes Monitor, Visibility, PDF tabs
- **Terminal theme override:** "Auto" (follows global theme) or explicit: Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight
- SettingsDialog preserves externalThemes list through `result()` method
- **Font size clamping:** all font sizes clamped to 6–72 range after loading to prevent invalid values from corrupted settings
- **Font intensity:** per-component opacity (0.3–1.0), clamped on load. `blendIntensity()` mixes text color towards background by factor. GUI intensity baked into global stylesheet `%2` textColor. Editor intensity applied via `SyntaxHighlighter::setIntensity()` (rebuilds highlighting rules with blended colors) + `CodeEditor` stylesheet. File browser intensity via `FileItemDelegate::setIntensity()` + `blendColor()` on all hardcoded per-filetype/git-status colors. Prompt/diff/changes via `objectName` + ID selectors in per-widget stylesheets. Terminal via `QTermWidget::setTerminalOpacity()`. `intensityFromSpin()` calls `interpretText()` to commit typed values before reading
- **Application-wide GUI font:** `qApp->setFont(guiFont)` sets default font for all widgets and dialogs. `applySettings()` additionally sets GUI font on tab bars, all buttons/labels/combos/checkboxes in bottom tabs (Notifications, Diff, Changes, Git, Search, Blame) via `findChildren<>()` loop. Notification list items inherit font at creation. No hardcoded font sizes in any widget

## Data Flow

1. **File Opening:** FileBrowser emits `fileOpened` -> MainWindow creates CodeEditor tab, adds to FileWatcher (no terminal cd)
2. **Prompt Sending:** PromptEdit emits `sendRequested` -> MainWindow logs to Project + sends to Terminal
3. **Save+Send:** PromptEdit emits `saveAndSendRequested` -> MainWindow saves prompt ID + sends to Terminal
4. **Git Commit:** Commit button (Git tab) -> themed dialog for message -> async QProcess chain: init -> add . -> commit -m "message" -> refresh git graph (non-blocking, shared_ptr for output)
5. **Settings:** SettingsDialog -> MainWindow applies `applyGlobalTheme()` first, then `applySettings()` to all components (language set before theme for single rehighlight)
6. **Git Status:** Timer (10s→60s adaptive) + QFileSystemWatcher (instant, monitors `.git/` directory + `.gitignore`) -> single `git --no-optional-locks status --porcelain --ignored` (git root cached) -> parse XY entries inline -> set comparison -> cache rebuild -> viewport update. SSH mode: same command via `ssh user@host` with ControlMaster multiplexing
7. **SSH Connect:** SshDialog -> MainWindow adds profile -> SshManager async mount -> profileConnected signal -> setup file browser + terminals
8. **SSH Disconnect:** onSshDisconnect() -> sshDisconnectTerminals() -> disconnectProfile(idx) -> doUnmount (async) + update activeIndex + stop health check -> emit profileDisconnected -> MainWindow clears SSH mount + restores local root
9. **SSH Reconnect:** Health timer -> async stat check -> connectionLost -> async fusermount + remount -> reconnected
9. **File Transfer:** Upload/Download via sshfs mount (rsync --progress or cp) -> transferProgress/transferFinished signals
10. **Tab Close:** maybeSaveTab() checks unsaved changes -> removeTab + widget->deleteLater() prevents memory leak
11. **Stop:** Stop button -> parse escape sequences -> sendText() to terminal
12. **Notifications:** Components call MainWindow::notify() -> NotificationPanel logs entry, updates unread badge (SSH events, transfers, git commits)
13. **Command Palette:** Ctrl+Shift+P -> CommandPalette.show() -> user selects command -> action callback executed
14. **Changes Monitor:** QFileSystemWatcher + scan timer (10s) -> changeDetected -> badge update on tab; user selects file -> git diff preview; Revert -> git checkout
15. **Theme Switch:** Settings/CommandPalette -> applyGlobalTheme() (global QSS) -> applySettings() (per-widget overrides) -> force repaint all children
16. **Git Graph:** Tab activation -> single `sh -c` with marker-separated git commands (branch -a, status -sb, remote -v, config, rev-list, log) -> parse sections -> O(n) BFS reachability -> GitGraphView.setCommits() -> QPainter render. SSH mode: same command via `ssh user@host` with ControlMaster
17. **Remote Operations:** Fetch/Pull/Push buttons -> async QProcess -> outputMessage signal -> NotificationPanel; Remotes dialog -> git remote add/set-url/rename/remove
18. **Markdown Preview:** Ctrl+M on .md file (or 👁 tab button) -> reuse warmup or create new MarkdownPreview instance -> add as tab -> connect editor.textChanged -> 500ms debounce -> markdownToHtml (cmark-gfm parse -> AST math injection -> render HTML) -> render() builds full HTML page -> setHtml() with base URL -> hljs.highlightAll() -> mermaid.run() -> katex.render() per span
19. **PDF Export:** Command Palette "Export Preview to PDF" -> QFileDialog for path -> injectPrintCss -> printToPdf (QByteArray callback) -> QPdfDocument count pages -> QPdfWriter+QPainter render each page (image + white margin overlays + optional border + optional page number) -> removePrintCss
20. **Fuzzy File Opener:** Ctrl+P -> FileOpener.show() -> scan project files (cached) -> fuzzy filter on keystroke -> Enter opens file via onFileOpened()
21. **Workspace Search:** Ctrl+Shift+F -> WorkspaceSearch.focusSearch() -> Enter triggers grep process -> results parsed (file:line:text) -> click shows context preview -> double-click emits fileRequested(path, line) -> MainWindow opens file and jumps to line
22. **Git Blame:** Blame tab activated (or Command Palette) -> blameCurrentFile() -> GitBlame.blameFile(workDir, filePath) -> async `git blame --porcelain` -> parse output -> BlameView.setBlameData() -> gutter paint with annotations

## Session Persistence

Stored in QSettings on application close:
- Window geometry via `normalGeometry()` (not `pos()` — handles maximized state correctly)
- Target screen identification by geometry match for multi-monitor setups
- Main splitter and right splitter positions
- File browser root path
- List of open file paths with per-file cursor positions and scroll positions
- Active editor tab index
- Active bottom tab index

## Security Model

- SSH passwords: sent via PTY (echo off) for terminals, password_stdin for sshfs (delivered after `started` signal), SSH_ASKPASS for tunnels
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
