# Changelog

All notable changes to Vibe Coder are documented in this file.

## [0.21.0] - 2026-04-02

### Added
- **Font intensity** — per-component text opacity control (30%–100%) for editor, file browser, prompt, diff viewer, changes monitor, terminal, and GUI. Blends text color towards background. Editor intensity affects syntax highlighting colors. File browser intensity affects all file type colors, git status colors, and directory colors via delegate. GUI intensity applies to global stylesheet text color (tabs, buttons, status bar, menus, dialogs). Terminal intensity uses QTermWidget opacity. Settings UI: percentage spinbox in each tab
- **Tab maximize on double-click** — double-clicking a tab bar (top or bottom) maximizes that panel to fill the entire window, hiding the file browser and the other panel. Double-click again to restore original splitter layout

### Fixed
- **Markdown preview first-open flicker** — first markdown preview caused window resize/flicker due to QWebEngineView Chromium process initialization. Fixed by pre-creating a hidden MarkdownPreview instance at startup; first preview reuses the pre-initialized instance
- **Diff/Changes viewer colors** — `setViewerColors()` was an empty stub (colors ignored). Now applies background and text colors via stylesheet with ID selectors
- **Font intensity not applied** — `qApp->setStyleSheet()` type selectors (`QWidget`, `QPlainTextEdit`) overrode per-widget stylesheets. Fixed by removing `color` from base type selectors and applying GUI-blended text color directly in global stylesheet. Per-widget intensity uses `objectName` + ID selectors for higher specificity
- **Settings spinbox value not saved on direct typing** — typing a value and clicking OK without pressing Enter didn't commit the spinbox value. Fixed by calling `interpretText()` before reading values
- **Deprecated `invalidateFilter()` warning** — replaced with `beginFilterChange()`/`endFilterChange()` in FileBrowserProxy

### Changed
- **Diff/Changes viewers now use dedicated font settings** — previously used GUI font; now use their own font family, size, and weight from Settings (Diff tab, Changes tab)
- **Markdown preview rendering** — switched from file-based rendering (`load(QUrl::fromLocalFile)`) to in-memory rendering (`setHtml()` with base URL) to reduce filesystem I/O

## [0.20.0] - 2026-04-01

### Changed
- **FileBrowser git status — single-command architecture** — replaced 5-step sequential QProcess pipeline (`rev-parse` → `rev-parse` → `git status` → `ls-files --ignored` → `check-ignore --stdin` with recursive 5000-entry dir scan) with a single `git --no-optional-locks status --porcelain --ignored` command. Git root is discovered once and cached (cleared on root path change). `--no-optional-locks` prevents blocking when other git processes are running. Debounce reduced from 300ms to 150ms
- **FileBrowser git watches** — replaced individual watches on `.git/index`, `.git/HEAD`, `.gitignore`, and root directory with a single watch on `.git/` directory (catches all git state changes) plus `.gitignore`. Watches are re-ensured after every git refresh to handle atomic saves (rename-over drops inotify watches)
- **SSH git operations run on remote server** — FileBrowser and GitGraph now execute git commands via `ssh user@host "cd /path && git ..."` instead of running git on the slow sshfs FUSE mount. SSH ControlMaster multiplexing (`ControlMaster=auto`, `ControlPath=/tmp/vibe-ssh-...`, `ControlPersist=120`) reuses a single SSH connection, eliminating repeated handshake overhead (~0.5-2s per call). Separate 1500ms debounce for SSH mode
- **GitGraph — single combined refresh** — replaced 5-7 sequential QProcess spawns (`loadBranches`, `loadLog`, `loadTrackingInfo`, `loadRemotes`, `loadUserInfo`) with a single `sh -c` command using marker-separated sections. All callers (tab activation, Refresh button, branch switch, Fetch/Pull/Push completion, Remotes dialog, User dialog) now use the unified `refresh()` method
- **GitGraph — O(n) commit reachability** — replaced O(n²) brute-force flood-fill (repeated full-array scans) with hash-map indexed stack-based BFS for marking remote-only commits
- **GitGraph — SSH-aware** — when SSH is active, git commands run directly on the remote server via SSH with ControlMaster multiplexing, avoiding sshfs FUSE overhead
- **FileBrowser header** — shows only directory name instead of full remote path when SSH is active
- **FileBrowser refresh button** — added animated refresh button next to directory name. Custom QPainter-drawn rotating arrow icon with spin animation on click (15°/30ms, 800ms duration). Matches height of directory button via shared layout
- **Modern UI overhaul** — complete QSS restyle inspired by Zed/VS Code: rounded buttons/inputs/combos (`border-radius: 6px`), rounded menus (`border-radius: 8px`) with rounded items, custom checkbox indicators, flat borderless tabs with accent underline, thin rounded scrollbars with hover effect, rounded tooltips/group boxes/progress bars, transparent label backgrounds, pressed/disabled button states
- **Status bar cleanup** — removed all borders (`QStatusBar::item { border: none }`), size grip disabled. SSH profile combo uses `AdjustToContents` instead of fixed width for dynamic sizing. Status info label moved from permanent to regular widget for better space allocation
- **Consistent GUI font across all bottom tabs** — `qApp->setFont(guiFont)` sets application-wide default. All buttons, labels, combos, checkboxes, and list items in Notifications, Diff, Changes, Git, Search, and Blame tabs now use the configured GUI font. Removed hardcoded `font-size: 11px` from GitGraph tracking label. Removed `setFixedWidth` from buttons in Notifications (Clear), Diff (Refresh), and Changes (Refresh, Clear) for dynamic sizing. Tab bars (`tabBar()`) explicitly set to GUI font. Notification list items inherit GUI font at creation time. Dialogs (Git Remotes, Git User, SSH, Settings) inherit font via `qApp->setFont()`
- **FileBrowser refresh triggers Git tab** — refresh button click emits `refreshRequested` signal, MainWindow refreshes GitGraph in response
- **Faster startup** — QWebEngine warmup deferred 500ms after window show (no parent, `WA_DontShowOnScreen` to prevent window flash). Terminal shell start deferred via `QTimer::singleShot(0)` so constructors don't block. Window maximize restored without double-show (`m_restoreMaximized` flag checked in `main.cpp`)
- **Prompt tab GUI font** — AI-terminal button and activity indicator now use GUI font

## [0.19.0] - 2026-03-30

### Fixed
- **SSH remote development over Tailscale/high-latency networks** — three issues caused the UI to freeze when connecting via sshfs:
  1. `cache=no,no_readahead` sshfs flags disabled all caching, forcing every `stat()` call to make a round-trip over the network. Removed these flags to allow kernel-level FUSE caching
  2. Git status operations (`git status`, `git check-ignore --stdin` with recursive directory walk up to depth 6) and filesystem watchers ran synchronously on the UI thread over sshfs. Git operations and filesystem watchers are now skipped on SSH mounts
- **sshfs password delivery** — password was written to sshfs stdin immediately after `QProcess::start()` before the process was ready. Now waits for the `started` signal before writing

## [0.18.0] - 2026-03-26

### Changed
- **Terminal directory behavior** — terminals now change directory only when a folder is opened via the Open Directory dialog. Navigating subdirectories by double-click or opening files in the file browser no longer triggers terminal `cd` commands. Added `rootPathOpenedByDialog` signal to FileBrowser to separate dialog-based navigation from tree navigation
- **Instant git status after commit/add/reset** — QFileSystemWatcher now monitors `.git/index` and `.git/HEAD` in addition to `.gitignore` and git root. `.git/index` is rewritten on every `git commit`, `git add`, `git reset`, and `git checkout`, so file browser colors update within 300ms instead of waiting for the next polling cycle (up to 60s). `.git/HEAD` changes on branch switch and commit. Watched files are automatically re-added after atomic writes (git uses rename-over which causes inotify to drop the watch)

### Fixed
- **SSH disconnect crash** — application crashed shortly after SSH disconnect. Root cause: `SshManager::disconnectProfile()` emitted `profileDisconnected` signal before updating `m_activeIndex`, so the MainWindow handler saw the profile as still active, skipped cleanup (clearSshMount, restore local root), and the file browser continued accessing the unmounted sshfs path. Fixed by moving the signal emission after all state updates (active index reset, health check stop)

## [0.17.0] - 2026-03-24

### Added
- **Application icon** — SVG icon with vibe wave motif and text cursor, embedded via Qt resources. Set as window icon (`QIcon(":/vibe-coder.svg")`) for taskbar and title bar

### Fixed
- **Multi-line comment highlighting** — C/C++, JavaScript, Rust: when a `/* */` comment spanned multiple lines and the closing `*/` was within the first 2 characters of a line (e.g. ` */` or `*/`), it was skipped due to incorrect search offset. The `startIndex + 2` offset (needed to avoid matching `/*` as `*/`) was also applied when continuing from a previous block where `startIndex = 0`. Now uses offset 0 when continuing from previous block, +2 only for newly found `/*`

## [0.16.0] - 2026-03-22

### Added
- **AI-terminal activity indicator** — animated spinner (`| / - \`) in prompt area shows when AI-terminal content is actively changing. Uses periodic screen snapshot comparison (`QWidget::grab()` + MD5 hash, 400ms interval) to detect content changes regardless of terminal visibility. Stops animating after 1.5s of idle. Works even when terminal tab is hidden
- **AI-terminal quick button** — "AI-terminal" button in prompt area to quickly switch to the AI-terminal tab
- **Active tab accent border** — selected tab now has a 2px bottom border in the theme's accent/selected color (`%6`), making the active tab clearly visible across all themes including those with subtle background differences
- **Minimal splitter handles** — splitter handles between panels (file browser, editor, prompt) reduced to 1px width/height for maximum screen space utilization

## [0.15.0] - 2026-03-21

### Added
- **External theme system** — all themes now loaded exclusively from `~/.config/vibe-coder/themes/`. No hardcoded built-in themes. Ships with 8 theme JSON files (Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord). Adding a new theme = dropping a `.json` file in the directory
- **Multi-format theme support** — auto-detects native JSON, Zed, and VS Code theme formats. Native format uses flat keys (`background`, `foreground`, `lineHighlight`, `terminalScheme`). Zed uses `themes[].style`. VS Code uses `colors` object with standard keys (`editor.background`, `sideBar.background`, `editor.lineHighlightBackground`, etc.)
- **Theme-driven line highlight** — current line highlight color in editor and prompt now comes from the active theme (`lineHighlight` field). Falls back to auto-computed color from background
- **Theme-driven file browser colors** — hover and selection colors in file browser delegate now use theme colors. Selected file uses `lineHighlight` color (same as editor current line)
- **File browser active file highlight** — switching editor tabs auto-selects and scrolls to the corresponding file in the file browser
- **Editor line spacing** — `SpacedDocumentLayout` (QPlainTextDocumentLayout subclass) overrides `blockBoundingRect` to apply configurable line spacing factor. Previous approach via `QTextBlockFormat::lineHeight` did not work with QPlainTextEdit
- **cmark-gfm integration** — replaced dlopen libcmark + regex fallback with statically linked cmark-gfm (GitHub Flavored Markdown). Native support for tables, strikethrough, autolinks, tagfilter. Math detection (`$...$`, `$$...$$`) now operates on AST text nodes only — code spans and code blocks are inherently excluded by the parser, eliminating all placeholder/masking bugs. ~450 lines of regex parsing code removed, replaced by ~80 lines of AST walking

### Fixed
- **Font weight not working** — Qt5 font weight values (25-75) were passed to Qt6 which uses 100-1000 scale. Migrated to `QFont::Thin/Light/Normal/Medium/DemiBold/Bold` enum values. Saved settings auto-migrated on load

### Removed
- `regexConvert()` — entire regex-based markdown parser (~200 lines)
- `processInline()` — regex inline formatting with placeholder protection (~80 lines)
- `extractAndConvertTables()` — manual table pre-extraction for plain cmark (~80 lines)
- `cmarkConvert()` — cmark wrapper with table placeholder restoration
- All placeholder logic (CODEBLK, MATH, TABLE, CODE masking/unmasking)
- `dlopen`/`dlsym` libcmark runtime dependency — cmark-gfm is now statically linked
- `libdl` link dependency

## [0.14.0] - 2026-03-19

### Added
- **Fuzzy File Opener (Ctrl+P)** — popup for quick file opening by name with fuzzy matching. Scans up to 50k files, skips build/cache directories (`.git`, `node_modules`, `build`, `dist`, `.cache`, `target`, etc.). Scoring prioritizes substring matches in filename over path. Arrow keys navigate, Enter opens, Escape dismisses. File list cached per root path
- **Workspace Search (Ctrl+Shift+F)** — full-text search across project files as bottom tab ("Search"). Uses system `grep` with `--max-count=500`. Options: case sensitive, regex/fixed string. Split view with result list + context preview (7 lines around match). Double-click opens file at line in editor
- **Git Blame** — per-line blame annotations as bottom tab ("Blame"). Auto-blames current file when tab is activated. Runs `git blame --porcelain`, displays commit hash, author, date in a gutter widget. Alternating background colors per commit. Refresh button for re-blame
- **Bracket matching** — highlights matching `()`, `{}`, `[]` pairs at cursor position in code editor. Depth-tracked for nested brackets, searches forward for opening brackets, backward for closing
- **Auto-close brackets** — typing `(`, `{`, `[`, `"`, `'` auto-inserts closing pair and positions cursor between them. Typing a closing bracket skips over existing one. Backspace between a pair deletes both characters

### Enhanced
- **Session restore** — now saves and restores per-file cursor positions and scroll positions, plus active bottom tab index. Previously only saved file paths and active editor tab

### Fixed
- **Markdown preview rewritten** — rendering switched from JS injection (`runJavaScript("updateBody(...)")`) to file-based loading (`setUrl()`) like text-ed. Eliminates JS string escaping issues that broke rendering of complex markdown with special characters
- **Math inside code blocks** — `$` characters inside backtick code spans (e.g. `` `^[a-zA-Z0-9._@-]+$` ``) were incorrectly matched as LaTeX math delimiters, causing large sections of text to disappear. Fixed by masking code spans and fenced code blocks before math detection
- **KaTeX rendering with cmark** — when libcmark was used, math expressions were restored as raw text without `<span>` wrappers, so KaTeX JS selectors (`.katex-display`, `.katex-inline`) never found them. Now both cmark and regex paths wrap math in proper KaTeX spans with `$$` delimiters stripped
- **Inline code vs bold/italic conflict** — content inside backtick code (e.g. `*`, `[`, `]`) was processed by bold/italic/link regex before code regex ran. Fixed with placeholder-based code protection: code spans are extracted first, other inline patterns applied, then code restored
- **PDF code block wrapping** — `@media print` CSS for `pre-wrap` is now embedded directly in the HTML page instead of injected via JS at export time, ensuring it's always applied. Long code lines wrap instead of being cut off
- **Mermaid blocks** — changed from `<pre class="mermaid">` to `<div class="mermaid">` (matching text-ed approach), with `suppressErrors` and error styling fallback
- **Regex converter rewritten** — two-pass approach (block elements + inline formatting) with `processInline()` modeled after text-ed: placeholder-based math and code protection, `toHtmlEscaped()` on non-protected text, then inline regex (bold, italic, strikethrough, images, links)

### New Files
- `src/fileopener.h/cpp` — FileOpener popup widget
- `src/workspacesearch.h/cpp` — WorkspaceSearch bottom tab widget
- `src/gitblame.h/cpp` — GitBlame + BlameView + BlameGutter widgets

## [0.13.0] - 2026-03-19

### Added
- **Widget style setting** — Settings > GUI > "Widget style" combo allows switching Qt widget style (Fusion, Windows, Breeze, Adwaita, Oxygen, Kvantum, etc.). Auto-detects all installed Qt6 style plugins via `QStyleFactory::keys()`. Style applied at startup in `main.cpp` before widget creation, and live-switchable from Settings. Default: "Auto" (system default)
- **Markdown table rendering with cmark** — `libcmark` (plain) does not support GFM tables, so tables are now pre-extracted from markdown, converted to HTML with alignment support (`:---:`, `---:`), replaced with placeholders before cmark processing, and restored after conversion. Works with both cmark and regex fallback
- **Improved code highlighting in preview** — highlight.js now runs on all `<pre><code>` blocks (not just those with `language-` class), enabling auto-detection for unlabeled code blocks

### Performance
- **Async git config** — `QProcess::execute()` (synchronous, blocking UI) replaced with async `QProcess::start()` + `finished` signal in git user/email editing. Uses `std::shared_ptr<int>` counter to coordinate parallel name+email updates, calls `loadUserInfo()` only after both complete

### Security
- **memfd_create for SSH passwords** — on Linux, SSH passwords are now written to an in-memory file descriptor via `memfd_create(MFD_CLOEXEC)` instead of `QTemporaryFile`. Password never touches disk. Accessed via `/proc/self/fd/N`. File descriptor auto-closed when parent QObject is destroyed. Falls back to `QTemporaryFile` on non-Linux systems or if `memfd_create` fails
- **JS asset integrity check** — bundled assets (mermaid.js, highlight.js, KaTeX) are now verified on every startup by comparing on-disk content against Qt resource originals. If tampered or corrupted, the file is overwritten with the known-good bundled copy. Previously, existing files were never re-checked after first extraction

### Fixed
- **Markdown tables not rendering** — tables were silently dropped by libcmark (no GFM extension support). Now pre-processed before cmark conversion
- **Code block colors overridden** — CSS `pre code { color: ... }` had higher specificity than highlight.js `.hljs` class. Changed to `pre code:not(.hljs)` so hljs themes apply correctly

### Changed
- **File size warning raised to 10 MB** — large file warning threshold increased from 5 MB to 10 MB. Files above 10 MB show a confirmation dialog before opening. Files above 1 MB still disable syntax highlighting for performance

### Documentation
- **dependencies.md** — new document listing all build and runtime dependencies with exact Debian 13 (trixie) package versions

## [0.12.0] - 2026-03-18

### Added
- **Multiple Markdown Previews** — each `.md` file can have its own independent preview tab (no more single-preview limit). Ctrl+M toggles preview for the current file. Closing the source editor auto-closes its preview
- **Markdown tab preview icon** — `.md` file tabs show a 👁 button on the left side for quick preview toggle without switching tabs first
- **Syntax-highlighted code blocks** — bundled highlight.js 11.9.0 with 46+ language modules: x86asm, armasm, avrasm, mipsasm, glsl, vhdl, verilog, cmake, dockerfile, makefile, gradle, groovy, kotlin, swift, dart, r, julia, matlab, fortran, delphi, ada, haskell, erlang, elixir, clojure, scheme, lisp, ocaml, fsharp, scala, lua, vim, latex, properties, ini, nginx, apache, protobuf, thrift, graphql, wasm, arduino, powershell, dos, tcl, awk (plus all core languages: C, C++, Python, JS, TS, Rust, Go, Java, etc.)
- **VS Code-style code themes** — VS2015 (dark) and VS (light) highlight.js themes for code blocks in markdown preview
- **Context-dependent zoom** — Ctrl+= / Ctrl+- now zooms only the focused component: code editor changes `editorFontSize`, prompt changes `promptFontSize`, file browser changes `browserFontSize`, markdown preview changes WebEngine zoom factor
- **Markdown preview zoom** — `zoomIn()`/`zoomOut()` methods on MarkdownPreview using QWebEngineView zoom factor (0.25x–5.0x range)

### Fixed
- **First-use preview flicker** — hidden QWebEngineView (0x0) pre-initializes Chromium engine at startup, eliminating the full-window flicker when opening the first markdown preview
- **dlopen memory leak** — if `dlsym` fails to find `cmark_markdown_to_html`, the library handle is now properly closed with `dlclose` instead of being leaked
- **Rename path traversal** — rename operation now validates against null bytes (`\0`) and newlines (`\n`), matching the validation in "New File" and "New Directory" operations
- **SSH tunnel process cleanup** — replaced `delete t.process` with `deleteLater()` on tunnel start failure to avoid deleting while signal handlers may be pending
- **SSH rsync check cleanup** — added `errorOccurred` handler to the async `which rsync` process to ensure cleanup if the process fails to start
- **Git remote async** — `git remote set-url`/`add` operations converted from blocking `waitForFinished()` to async `QProcess::finished` signal (no more UI freeze)

### Security
- **WebEngine hardening** — disabled `PluginsEnabled`, `PdfViewerEnabled`, and `NavigateOnDropEnabled` in markdown preview to reduce attack surface

### Dependencies
- **highlight.js** — bundled as Qt resource (`src/resources/highlight.min.js`, ~270KB with 46 language modules)
- **highlight.js themes** — VS2015 dark (`hljs-dark.min.css`) and VS light (`hljs-light.min.css`)

### Documentation
- **README** — fixed Debian package name (`libqtermwidget-dev` instead of `libqtermwidget6-1-dev`), added `qt6-webchannel-dev` to Debian deps, removed unnecessary `libutf8proc-dev` and `utf8proc-devel`, updated feature descriptions for multiple previews, syntax highlighting, context-dependent zoom, offline operation
- **Architecture** — updated for multiple preview instances, highlight.js integration, KaTeX math support, security hardening, context-dependent zoom
- **Diagrams** — updated class diagram and markdown preview flow for multi-instance architecture

## [0.11.0] - 2026-03-14

### Added
- **PDF Export from Markdown Preview** — export current preview to PDF via Command Palette ("Export Preview to PDF"). Renders exactly as shown in preview using `QWebEnginePage::printToPdf`. Two-pass rendering: first generates base PDF, then post-processes with `QPdfDocument` + `QPdfWriter` + `QPainter` for page numbers and border overlay
- **PDF Settings tab** — new "PDF" tab in Settings dialog with:
  - Orientation: portrait / landscape
  - Left margin (0–50mm, default 15mm)
  - Right margin (0–50mm, default 15mm)
  - Page numbering: none / page / page/total (rendered via QPainter at bottom center)
  - Page border: optional border around content area
- **Print-safe text wrapping** — `@media print` CSS injected before PDF export: `pre`/`code` use `white-space: pre-wrap`, tables use `table-layout: fixed` with `word-wrap: break-word`

### Fixed
- **Preview tab crash** — closing all tabs except markdown preview, then closing preview tab caused crash. `closeTab()` now checks if closed widget is `m_mdPreviewEditor` (clears dangling pointer) or `m_mdPreview` (hides without deleting)
- **PDF page border artifact** — QPainter renders white rectangles over margin areas (with 3px overlap into content) to cover body background edge artifacts from Chromium's `printToPdf`. Border only drawn when explicitly enabled in Settings

### Dependencies
- **Qt6::PdfWidgets** — new required dependency for PDF post-processing (page counting, re-rendering with page numbers)

## [0.10.0] - 2026-03-14

### Added
- **Markdown Preview (Ctrl+M)** — live preview for `.md` files as editor tab. Uses QWebEngineView with bundled mermaid.js (~3MB) for diagram rendering. libcmark for markdown conversion (dlopen, optional) with regex fallback. 500ms debounced live updates. Dark/light theme variants
- **Mermaid diagram rendering** — `\`\`\`mermaid` code blocks rendered as actual diagrams via embedded mermaid.js. Works fully offline, no CDN dependencies
- **Commit dialog** — commit button moved to Git tab toolbar. Shows themed dialog with editable commit message (default: timestamp). Runs `git add .` + `git commit -m "message"`
- **Git user info** — User button in Git tab toolbar shows `git config --global user.name/email`. Click to edit via themed dialog
- **File browser drag & drop** — files and directories can be moved by dragging within the tree view. Custom `FileBrowserTreeView` subclass handles drop events with filesystem rename
- **File browser simplified** — removed path text field, replaced with "Open Directory…" button showing current directory name

### Performance
- **Markdown regex optimization** — all 7 inline regex patterns pre-compiled as `static` (compile once, reuse). Horizontal rule regex also static
- **Diff caching** — ChangesMonitor caches last diff result per file, invalidates on file change
- **QFileSystemWatcher error checking** — `addPaths()` return value checked, warns on platform limit failures

### Security
- **SSH tunnel host validation** — `remoteHost` validated with `^[a-zA-Z0-9._-]+$` regex before use
- **SSH tunnel direction validation** — must be exactly "local" or "remote", rejects unknown values
- **SSH mount point TOCTOU fix** — random suffix via `QRandomGenerator` prevents race condition on temp directory creation
- **WebEngine isolation** — `LocalContentCanAccessRemoteUrls` disabled, prevents markdown content from making network requests

### Fixed
- **Terminal paths with spaces** — all `cd` commands now quote paths with double quotes. Fixes directories like `__pre github/_IDE-ed`
- **First-use preview flicker** — `MarkdownPreview` (QWebEngineView) pre-created hidden at startup, Chromium subprocess initializes early
- **Black square on startup** — preview widget hidden with `setVisible(false)` until added to tab

### Memory & Resources
- **Commit chain memory safety** — replaced `new QString` with `std::make_shared<QString>`. Added `QProcess::errorOccurred` handler for cleanup on crash
- **Preview widget lifecycle** — created once, reused across toggle cycles (no repeated QWebEngineView allocation)

### Dependencies
- **Qt6::WebEngineWidgets** — new required dependency for markdown preview
- **libdl** — for dlopen of libcmark (optional runtime dependency)
- **mermaid.js** — bundled as Qt resource (`src/resources/mermaid.min.js`, ~3MB)

## [0.9.0] - 2026-03-14

### Added
- **Git Graph tab** — visual commit history with QPainter-drawn graph (colored lanes, merge curves, branch labels). Branch selector with local + remote branches. Auto-refreshes on directory change
- **Remote operations** — Fetch, Pull, Push buttons in Git tab with loading state feedback
- **Upstream tracking info** — shows `branch -> upstream [ahead N, behind M]` with color coding in Git tab toolbar
- **Remotes management dialog** — view, add, edit (rename + set-url), remove multiple git remotes
- **Terminal theme setting** — Settings > Terminal > Theme: "Auto" (follows global theme) or explicit override (Linux, BlackOnWhite, DarkPastels, Solarized, SolarizedLight)
- **ThemedMessageBox** — custom frameless message box replacing QMessageBox for consistent dark theme support across all dialogs
- **Tab close icons** — dynamically generated cross icons matching theme text color, applied via stylesheet

### Performance
- **O(1) find navigation** — `onFindNext()`/`onFindPrev()` use counter increment instead of O(n) document re-scan
- **Large file mode** — files >1MB disable syntax highlighting and line wrapping
- **Git timer debounce** — 10s timer skips refresh if previous git operation still running
- **feedEntries limit** — max 5000 files fed to git check-ignore stdin
- **Diff output cap** — diff viewer and changes monitor truncate output at 512KB
- **Scan limit** — changes monitor file scan capped at 5000 files

### Security
- **SSH identifier allowlist** — replaced denylist regex with `^[a-zA-Z0-9._@-]+$` (max 253 chars)
- **SSH identity file validation** — checks file existence before use
- **Path traversal prevention** — SSH path edit validates canonical path stays within mount point
- **Git stdin injection** — file names filtered for newline/carriage-return/null bytes before feeding to git
- **File name validation** — creation/rename rejects null bytes and newlines

### Fixed
- **Dialog title bars white in dark theme** — replaced all QMessageBox usage with ThemedMessageBox (frameless + themed)
- **Tab close icons invisible in dark theme** — generated theme-colored PNG icons via QPainter
- **Global stylesheet scope** — changed from `setStyleSheet(ss)` to `qApp->setStyleSheet(ss)` so all dialogs inherit theme
- **Git colors not updating** — fixed QProcess kill/restart race by waiting for `finished` signal before restarting

### Memory & Resources
- **QProcess crash leak** — added `errorOccurred→deleteLater()` to all async QProcess instances (diffviewer, changesmonitor, gitgraph)
- **Git output capping** — status/ls-files/check-ignore output truncated at 2MB
- **QFileSystemWatcher limits** — max 4000 entries to stay within OS limits
- **ChangesMonitor skip patterns** — added build, .cache, target, dist, vendor to excluded directories
- **Font size clamping** — settings load clamps all font sizes to 6–72 range

### Error Handling
- **Project save** — `save()` returns bool, checks `file.write()` return value
- **File operations** — mkdir and removeRecursively show warning dialogs on failure

## [0.8.0] - 2026-03-13

### Added
- **Custom title bar (CSD)** — VS Code/Zed-style frameless window with custom minimize/maximize/close buttons. Title bar matches the active theme. Native window drag via `startSystemMove()` and edge resize via `startSystemResize()` (Wayland compatible)
- **Themed dialogs** — all dialogs (Settings, SSH Connect, SSH Tunnels, Create/Edit Project) use frameless windows with themed title bars via `ThemedDialog::apply()`
- **Zed theme loader** — auto-discovers and imports Zed editor themes from Flatpak (`~/.var/app/dev.zed.Zed/...`) and native (`~/.local/share/zed/...`) installations. Themes appear in Settings and command palette prefixed with "Zed: "
- **Dark Soft theme** — dark theme with dimmer text colors (bg: #1a1a2e, text: #a0a0b8, terminal: DarkPastels)
- **Dark Warm theme** — dark theme with brownish tones and low-intensity colors (bg: #1e1a15, text: #6e6458, terminal: DarkPastels)
- **Instant git status updates** — QFileSystemWatcher monitors root path, git root, and .gitignore for immediate color changes (300ms debounce), complementing the 10s polling interval
- **Git check-ignore pipeline** — `git check-ignore --no-index --stdin` with recursive file enumeration (depth 6) detects ignored files regardless of tracking status. Combined with `git ls-files --others --ignored --exclude-standard --directory` for comprehensive ignore detection

### Changed
- **Unified theme system** — single global theme controls all UI components. Removed individual color settings (prompt colors, diff colors, changes colors, editor color scheme combo, browser theme combo). All colors derived from `applyThemeDefaults()` based on `globalTheme`
- **Theme application order** — `applyGlobalTheme()` (global QSS) runs before `applySettings()` (per-widget overrides) to ensure line numbers, editor colors, and other widget-specific styles take precedence over the global stylesheet
- **Comprehensive global stylesheet** — covers QWidget, QLabel, QPlainTextEdit, QTextEdit, QLineEdit, QSpinBox, QComboBox, QCheckBox, QPushButton, QToolButton, QListWidget, QMenu, QDialog, QGroupBox, QScrollBar, QProgressBar, QTabWidget, QTabBar, QFontComboBox, QDialogButtonBox
- **Line number area styling** — uses explicit stylesheet override (background-color + color) instead of relying on QPalette inheritance, preventing global stylesheet from overriding colors
- **Multi-monitor window restore** — uses `normalGeometry()` instead of `pos()` for saving, finds target screen by geometry match, uses `setGeometry(screenGeom)` + deferred `showMaximized()` to prevent window jumping to primary monitor
- **Light theme terminal** — uses BlackOnWhite color scheme (was WhiteOnBlack)
- **Solarized Light terminal** — uses SolarizedLight color scheme (was Solarized/dark)
- **FileBrowser theme** — accepts bg/fg color parameters from global theme, uses `darker()` for light themes instead of `lighter()` (prevents near-white paths)
- Removed hardcoded stylesheets from CommandPalette, NotificationPanel, ChangesMonitor, DiffViewer — all inherit from global stylesheet
- Removed ColorButton widget class (no longer needed)

### Fixed
- **Prompt font size not persisting** — `setStyleSheet()` was overriding `setFont()` due to Qt CSS cascade. Fixed by including font-family/font-size in the stylesheet string
- **Gitignored files not grayed** — `git status --ignored` didn't detect all ignored files. Fixed with multi-step pipeline: git status + git ls-files + git check-ignore
- **Parent directory incorrectly grayed** — ignored status propagated upward in `rebuildDirCache()`, making entire directories gray when only one child was ignored. Fixed by removing ignored propagation to parents
- **Untracked directory children not colored** — `git status -unormal` reports `?? dir/` but files inside weren't matched. Added child lookup traversal for untracked/added/modified sets
- **Window jumping to primary monitor** — `restoreGeometry()` + `showMaximized()` moved window to primary monitor. Fixed with manual geometry save/restore and screen detection
- **Line numbers white strip** — CodeEditor used hardcoded QPalette colors overridden by global stylesheet. Fixed by passing theme bg/fg colors to `setEditorColorScheme()` and setting explicit stylesheet on line number area
- **Menus not following theme** — many widgets had hardcoded colors or relied on system theme. Fixed by comprehensive global stylesheet
- **Settings dialog not themed** — was using system window decorations. Fixed with ThemedDialog + parent stylesheet inheritance

## [0.7.0] - 2026-03-13

### Added
- **Stop button** — sends configurable stop sequence to terminal (default: Ctrl+C `\x03`). Configurable in Settings > Prompt as "Stop sequence" — supports escape sequences (`\x03`, `\x1b`, `\n`)
- **Find & Replace** — Ctrl+F opens find bar, Ctrl+H opens find & replace in code editor
  - Yellow highlight of all matches in text (via syntax highlighter overlay)
  - Yellow markers on vertical scrollbar showing match positions (custom MarkerScrollBar)
  - Match counter ("3 of 12"), navigate with ▲/▼ or Enter/Shift+Enter
  - Replace one / Replace All with undo group support
  - Escape closes find bar and clears highlights
  - Find bar respects dark/light editor theme
- **Undo/Redo** — built-in QPlainTextEdit support (Ctrl+Z / Ctrl+Shift+Z)
- **Unsaved changes tracking** — `●` marker in tab title when file content differs from saved version
  - Compares actual text content (undo back to original = marker disappears)
  - Tab close prompts Save / Discard / Cancel dialog
  - App close prompts Save All / Discard / Cancel when any tab has unsaved changes
- **Current line highlighting** — configurable per-component in Settings
  - Editor: Settings > Editor > "Highlight current line"
  - Prompt: Settings > Prompt > "Highlight current line"
  - Adapts highlight color to dark/light scheme
- **Visibility settings** — new Settings > Visibility tab
  - Gitignored files: visible / grayed / hidden
  - .git directory: visible / grayed / hidden (default: hidden)
  - Uses QSortFilterProxyModel for local files, filters in SSH model
  - Gray coloring only for explicitly configured items (no implicit graying of dotfiles)
- **Tab context menu** — right-click on tab bar shows:
  - Close, Close Others, Close All, Close to the Right, Close to the Left
  - All operations check for unsaved changes before closing

### Performance
- **Search debouncing** — 200ms delay before scanning document on keystroke (eliminates UI stutter during fast typing)
- **Syntax highlighter** — `setSearchPattern()` skips `rehighlight()` if pattern unchanged
- **Search results** — pre-allocated vector with `reserve(256)`, single-pass block iteration
- **Line number painting** — reuses QString buffer via `setNum()`, caches font metrics and area width
- **Git polling** — interval increased from 5s to 10s, timer only starts after git repo confirmed, stops when no repo
- **SSH health check** — interval increased from 5s to 15s (matches sshfs ServerAliveInterval)
- **File change debouncing** — 300ms debounce for QFileSystemWatcher signals (prevents multiple reloads per save)
- **Changes monitor** — scan interval increased from 3s to 10s, only scans when widget is visible, directory changes debounced via timer restart
- **Proxy model** — uses `beginFilterChange()/endFilterChange()` instead of model reset (preserves tree expansion state and scroll position)

### Fixed
- **Git status on startup** — git refresh for previous root path blocked the refresh for the restored session path (`m_gitBusy` race). Now cancels in-progress git process when root path changes
- **File browser sorting** — directories now always appear before files (FileBrowserProxy `lessThan()` override with case-insensitive sorting)

### Changed
- Default settings updated to match current preferences:
  - Global theme: Light (was Dark)
  - Terminal: Adwaita Mono 14pt, BlackOnWhite (was Monospace 10pt, Linux)
  - Editor: Monaco 14pt, Light scheme (was Monospace 10pt, Dark)
  - File browser: Noto Sans 12pt, Light (was Sans 10pt, Dark)
  - Prompt: Monospace 14pt, white background (was 10pt, dark background)
  - Diff/Changes: Monospace 13pt, white background (was 10pt, dark background)
- Terminal font applied via `setFamily()` + `setStyleHint(QFont::Monospace)` for exact font matching
- Terminal font reapplied after event loop via `QTimer::singleShot(0)` to override QTermWidget reset
- Burger menu disabled items now use distinct gray color (`QMenu::item:disabled`)
- Dotfile coloring removed — files starting with `.` no longer implicitly grayed in file browser

## [0.6.0] - 2026-03-12

### Added
- **Changes Monitor** — real-time file change tracker as bottom tab
  - QFileSystemWatcher + periodic scan (3s) detects modifications, new files, deletions
  - Split view: file list with timestamps + diff preview with syntax highlighting
  - Revert button: `git checkout -- <file>` to restore git version
  - Open button: opens changed file in editor tab
  - Badge count on tab: "Changes (N)"
  - Skips .git, .LLM, node_modules, __pycache__ directories
  - Watches up to 2000 files / 500 directories
- **Git ignored file coloring** — ignored files shown in dimmed color (dark: #5a5a5a, light: #b0b0b0) in file browser
- Git status now includes `--ignored` flag for full status visibility
- **Tabbed Settings dialog** — sections reorganized into tabs (Terminal, Editor, File Browser, Prompt, Diff Viewer, Changes Monitor)
- **Diff Viewer settings** — configurable font family, font size, background and text colors
- **Changes Monitor settings** — configurable font family, font size, background and text colors
- Global theme `applyThemeDefaults()` now cascades to diff viewer and changes monitor colors

### Changed
- Settings dialog default color scheme changed from Light to Dark
- Default prompt colors changed to dark theme (#1e1e1e bg, #d4d4d4 text)
- Settings dialog minimum size increased to 460×480 for tabbed layout
- Editor splitter wraps main tab widget (enables proper split view)

### Removed
- OutputParser and OutputPanel (AI terminal output parsing) — removed from codebase

## [0.5.0] - 2026-03-12

### Performance
- sshfs mount is now fully async — UI never blocks during SSH connect
- SSH health check runs async sequentially (one profile at a time, non-blocking)
- Git commit runs async via chained QProcess callbacks (no UI freeze)
- `which rsync` result cached — called only once per session
- Single rehighlight pass when opening files (setLanguage builds rules, setDarkTheme triggers rehighlight)
- Git status polling interval increased from 2s to 5s to reduce CPU usage
- Terminal shell start deferred until widget is placed in layout

### Security
- SSH passwords never appear in process argument lists (no sshpass -p)
- Transfers use sshfs mount (cp/rsync locally) — no SSH password needed
- Tunnels use SSH_ASKPASS mechanism with temporary script for password auth
- SSH user/host validated against shell metacharacters before use (command injection prevention)
- Git commit auto-adds .env, *.pem, *.key, credentials.json, id_rsa, id_ed25519 to .gitignore

### Memory
- Tab close properly deletes CodeEditor widget via deleteLater() (was leaking on every tab close)

## [0.4.0] - 2026-03-12

### Added
- SSH reconnect on connection loss: health timer (5s) with stat-based mount check, up to 3 auto-retry attempts
- Multiple simultaneous SSH profiles with profile switcher combo in status bar
- SFTP upload/download with rsync progress bar in status bar
- SSH port forwarding management: Local (-L) and Remote (-R) tunnels via SshTunnelDialog
- SshManager class: centralized SSH profile, health, transfer, and tunnel management
- SshTunnelDialog: create/view/remove active tunnels with QTableWidget

### Added (Prompts)
- Saved/recurring prompts per project (stored as IDs in instructions.json)
- Shift+Enter (or Ctrl+Shift+Enter) = send + save prompt
- Save Prompt button + delete (X) button for saved prompts
- Saved prompts combo with selection to fill prompt editor

### Added (SSH)
- SSH password support (masked input, never persisted)
- Saved SSH connections via QSettings (password excluded)
- Terminals auto-cd when opening remote files
- Path bar shows and accepts remote paths

### Added (File Browser)
- Dual model: QFileSystemModel (local) + QStandardItemModel (SSH)
- SSH file browsing via synchronous QDir reads over sshfs (reliable over FUSE)
- Lazy-load subdirectories on expand (dummy placeholder → real contents)
- sshfs mounts remote root (/) so all remote paths are accessible

### Added (UI)
- Bottom tab widget with Prompt and Terminal tabs
- AI-terminal (top) + Terminal (bottom) dual terminal layout

## [0.3.0] - 2026-03-12

### Performance
- Git status polling is now fully async (non-blocking QProcess pipeline)
- Replaced `-uall` with `-unormal` in git status for faster execution on large repos
- Directory git status uses precomputed cache — O(1) lookup instead of O(n) iteration
- File open warns on files larger than 5MB to prevent UI slowdown

### Security
- `.LLM/` directory automatically added to `.gitignore` on project creation
- Path traversal protection in New File, New Directory, and Rename operations
- Delete confirmation now shows full file path
- Delete dialog defaults to "No"

### Fixed
- File save (Ctrl+S) now shows error message on failure (permissions, disk full)
- File save confirms success in status bar
- Context menu respects file browser Dark/Light theme
- Prompt numbering: each prompt in instructions.json now has sequential `id` field

## [0.2.0] - 2026-03-12

### Added
- File browser Dark/Light theme setting (replaced raw color pickers)
- Editable text editor (was read-only)
- Ctrl+S to save current file
- Smart file reload: skips if content matches (avoids overwriting user edits)
- File browser Zed editor-style rendering with git status colors
- Git status colors propagate to parent directories
- Context menu: New File, New Directory, Rename, Delete
- Session persistence: window geometry, splitter sizes, open files, active tab

### Changed
- File browser settings use theme combo instead of color buttons

## [0.1.0] - 2026-03-12

### Added
- Initial release
- Qt6 Widgets application with horizontal/vertical splitter layout
- File browser (left) with QFileSystemModel, shows hidden files
- Terminal emulator (QTermWidget) supporting interactive apps (claude, mc, vim)
- Code editor tabs with syntax highlighting (C/C++, Python, JS/TS, Rust)
- Line number area with dark/light theme support
- Prompt input window with Send and Commit buttons
- Configurable send key: Ctrl+Enter or Enter
- Hamburger menu: Create Project, Edit Project, Settings
- Project system: .LLM/instructions.json with prompt history
- Git commit integration: auto-init, add -A, commit with timestamp
- Comprehensive Settings dialog: Terminal, Editor, File Browser, Prompt
- Terminal follows file browser directory changes
