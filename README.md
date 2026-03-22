# Vibe Coder

A Qt6 C++ IDE-like application for AI-assisted development workflows. Combines a Zed-style file browser, code editor with syntax highlighting, dual embedded terminals, SSH remote development, and prompt system in a single window with a custom title bar.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt6](https://img.shields.io/badge/Qt-6-green)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

## Features

- **File Browser** — Zed editor-style tree view with git status colors (modified, untracked, added, ignored), context menu (new file/dir, rename, delete), drag & drop file moving. Configurable visibility for gitignored files and .git directory (visible/grayed/hidden). Instant git status updates via QFileSystemWatcher + 10s polling. Works over SSH via sshfs
- **Code Editor** — Tabbed editor with syntax highlighting for C/C++, Python, JavaScript/TypeScript, Rust. Line numbers, Ctrl+S save. Split view (horizontal/vertical). Find & Replace (Ctrl+F/Ctrl+H) with yellow match highlighting and scrollbar markers. Undo/Redo. Unsaved changes tracking with `●` tab marker. Configurable current line highlighting. **Bracket matching** — highlights matching `()`, `{}`, `[]` pairs at cursor. **Auto-close brackets** — typing `(`, `{`, `[`, `"`, `'` auto-inserts the closing pair; Backspace between a pair deletes both; typing a closing bracket skips over it if already present
- **Dual Terminals** — AI-terminal (top, sends prompts) + general Terminal (bottom tab). Both follow file browser directory and support SSH. Stop button sends configurable stop sequence (default: Ctrl+C). **AI-terminal activity indicator** — animated spinner in prompt area shows when AI-terminal content is changing; stops when terminal is idle. Works even when terminal tab is hidden (screen snapshot comparison via `QWidget::grab()` + MD5 hash)
- **Prompt System** — Prompt input with configurable send key (Enter / Ctrl+Enter). Shift modifier = send + save. Saved/recurring prompts per project. Quick "AI-terminal" button to switch to terminal tab
- **Project Management** — `.LLM/instructions.json` stores project metadata, numbered prompt history, and saved prompt IDs. Auto-creates `.gitignore`
- **Markdown Preview** — Ctrl+M opens live preview for `.md` files. Multiple previews can be open simultaneously (one per .md file). Markdown tab icon (👁) for quick toggle. Powered by **cmark-gfm** (GitHub Flavored Markdown) with native support for tables, strikethrough, autolinks. Syntax-highlighted code blocks via bundled highlight.js (46+ languages). KaTeX math rendering ($$...$$ and $...$) via AST-level injection (code spans never interfere with math). Mermaid diagram rendering. Dark/light themes. File-based rendering (write HTML to temp file + setUrl). 500ms debounced live updates. Context-dependent zoom (Ctrl+=/Ctrl+-). **PDF Export** via Command Palette — renders preview to PDF with embedded print CSS for code wrapping, configurable margins, orientation, page numbering, and optional page border
- **Git Integration** — Commit button in Git tab with editable commit message dialog (auto-init + add + commit). Auto-filters sensitive files (.env, *.pem, *.key). Async git status with instant updates for file browser colors. **Git Graph tab** with visual commit history, branch/merge visualization, Fetch/Pull/Push, upstream tracking, git user info editing, and multi-remote management. **Git Blame** — per-line blame annotations with commit hash, author, and date in a dedicated bottom tab; auto-blames current file on tab switch
- **Changes Monitor** — Real-time file change tracking with diff preview (cached) and one-click revert to git version
- **Diff Viewer** — Git diff viewer with syntax highlighting (working changes, staged, last commit)
- **SSH Remote Development**
  - Multiple simultaneous SSH profiles with profile switcher
  - sshfs-based file browsing (synchronous QStandardItemModel for reliability)
  - Auto-reconnect on connection loss (health check every 15s, up to 3 retries)
  - File upload/download via sshfs mount with rsync progress bar
  - SSH port forwarding management (local -L / remote -R tunnels)
  - Saved connections (passwords never persisted, memfd_create on Linux)
  - Terminals auto-cd when opening remote files
- **Fuzzy File Opener** — Ctrl+P for quick file opening by name with fuzzy matching. Scans up to 50k files, skips build/cache directories, scores by substring and filename match
- **Workspace Search** — Ctrl+Shift+F for full-text search across the entire project. Case-sensitive and regex options, result preview with context, double-click to open file at line
- **Command Palette** — Ctrl+Shift+P for fuzzy-searchable commands (split view, focus, themes, diff refresh, blame, search)
- **Notifications** — Centralized log with Info/Warning/Error/Success levels and unread badge
- **Minimal Splitter Handles** — 1px thin separator lines between panels for maximum screen utilization
- **Active Tab Indicator** — selected tab highlighted with accent color bottom border for clear visibility across all themes
- **Custom Title Bar** — VS Code/Zed-style frameless window (CSD) with themed minimize/maximize/close buttons. All dialogs use themed title bars
- **Global Themes** — External theme system loading from `~/.config/vibe-coder/themes/`. Ships with 8 themes (Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord). Supports native JSON, Zed, and VS Code theme formats. Live switching without restart. Theme colors control editor line highlight, file browser selection, and all UI elements
- **Widget Styles** — Configurable Qt widget style (Fusion, Windows, Breeze, Adwaita, Oxygen, Kvantum). Auto-detects installed Qt6 style plugins. Affects button shapes, scrollbars, checkboxes, and other GUI component rendering
- **Settings** — Tabbed dialog with configurable fonts, sizes, global theme, and widget style for all components (terminal, editor, file browser, prompt, diff viewer, changes monitor, visibility, PDF export)
- **Session Persistence** — Remembers window size, splitter positions, open files, active tab, cursor positions, scroll positions, and active bottom tab. Multi-monitor aware
- **Fully Offline** — cmark-gfm statically linked, all resources (mermaid.js, KaTeX, highlight.js, fonts) bundled with integrity verification on startup. No network requests or runtime library dependencies. WebEngine remote URL access explicitly disabled

## Installation

### Debian / Ubuntu

```bash
# Install dependencies
sudo apt update
sudo apt install -y \
    build-essential cmake \
    qt6-base-dev \
    qt6-webengine-dev \
    qt6-webchannel-dev \
    qt6-pdf-dev \
    libqtermwidget-dev \
    sshfs

# Clone and build
git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./vibe-coder

# Copy themes to user config
mkdir -p ~/.config/vibe-coder/themes
cp ../themes/*.json ~/.config/vibe-coder/themes/
```

> **Note:** `sshfs` is required for SSH remote file browsing and transfers. `qt6-webengine-dev` requires `qt6-webchannel-dev` as a dependency on some Debian versions. `qt6-pdf-dev` is required for PDF export with page numbering. On older Debian/Ubuntu the qtermwidget package may be named `libqtermwidget6-1-dev` instead of `libqtermwidget-dev`. cmark-gfm is automatically downloaded and statically linked during cmake configure (requires internet at build time).

### Fedora

```bash
sudo dnf install -y \
    gcc-c++ cmake \
    qt6-qtbase-devel \
    qt6-qtwebengine-devel \
    qt6-qtpdf-devel \
    qtermwidget-devel \
    fuse-sshfs

git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)

# Copy themes to user config
mkdir -p ~/.config/vibe-coder/themes
cp ../themes/*.json ~/.config/vibe-coder/themes/
```

### Arch Linux

```bash
sudo pacman -S --needed \
    base-devel cmake \
    qt6-base \
    qt6-webengine \
    qt6-pdf \
    qtermwidget \
    sshfs

git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)

# Copy themes to user config
mkdir -p ~/.config/vibe-coder/themes
cp ../themes/*.json ~/.config/vibe-coder/themes/
```

## Usage

1. **Open a directory** — Click "Open Directory…" button in the file browser
2. **Edit files** — Click a file to open it in a new tab. Ctrl+S to save. Ctrl+F to find, Ctrl+H to find & replace. Ctrl+Z / Ctrl+Shift+Z for undo/redo
3. **Use the terminal** — AI-terminal (top) for prompts, Terminal tab (bottom) for general use
4. **Send prompts** — Type in the prompt area, press Ctrl+Enter (or Enter, configurable in Settings)
5. **Save prompts** — Shift+Enter (or Ctrl+Shift+Enter) sends and saves prompt for reuse
6. **Create a project** — Hamburger menu (☰) → Create Project. Stores config in `.LLM/instructions.json`
7. **SSH Connect** — Hamburger menu (☰) → SSH Connect. Enter user/host/password or select saved connection
8. **SSH Tunnels** — Hamburger menu (☰) → SSH Tunnels. Create local (-L) or remote (-R) port forwards
9. **Upload/Download** — Hamburger menu (☰) → SSH Upload/Download. Transfers via sshfs mount with progress
10. **Commit** — Click Commit in Git tab. Enter commit message (default: timestamp), auto-initializes git if needed, stages all files, commits
11. **Quick Open** — Ctrl+P to fuzzy-search and open any file in the project by name
12. **Workspace Search** — Ctrl+Shift+F to search text across all project files. Double-click a result to jump to the file and line
13. **Git Blame** — Open a file, then click the "Blame" tab at the bottom (or Command Palette → "Git Blame Current File") to see per-line blame annotations
14. **Command Palette** — Ctrl+Shift+P to search and execute commands (split view, focus panels, switch themes, blame, search)
15. **Changes Monitor** — Bottom "Changes" tab shows real-time file modifications with diff preview and revert
16. **Git Graph** — Bottom "Git" tab shows commit history as a visual graph. Use Fetch/Pull/Push buttons, manage remotes via "Remotes" button
17. **Settings** — Hamburger menu (☰) → Settings. Tabbed dialog for fonts, themes, and behavior
18. **Custom Themes** — Place `.json` theme files in `~/.config/vibe-coder/themes/`. Supports native, Zed, and VS Code formats
19. **Markdown Preview** — Open a `.md` file and press Ctrl+M (or click the 👁 icon on the tab). Multiple previews can be open at once. Press Ctrl+M again to close the current preview
20. **Export to PDF** — With preview open, use Command Palette (Ctrl+Shift+P) → "Export Preview to PDF". Configure margins, orientation, page numbering, and border in Settings > PDF
21. **Git User** — Click "User" in Git tab to view/edit git global name and email
22. **Zoom** — Ctrl+= / Ctrl+- zooms only the focused component (editor, prompt, file browser, or markdown preview)

## Documentation

- [Architecture](docs/architecture.md) — Component design and data flow
- [Diagrams](docs/diagrams.md) — Mermaid diagrams of class hierarchy, signal flow, and async pipelines
- [Dependencies](docs/dependencies.md) — Build and runtime dependencies with Debian 13 package versions
- [Changelog](docs/changelog.md) — Version history

## Author

**krse**

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
