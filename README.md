# Vibe Coder

A Qt6 C++ IDE-like application for AI-assisted development workflows. Combines a Zed-style file browser, code editor with syntax highlighting, dual embedded terminals, SSH remote development, and prompt system in a single window with a custom title bar.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt6](https://img.shields.io/badge/Qt-6-green)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

## Features

- **File Browser** — Zed editor-style tree view with git status colors (modified, untracked, added, ignored), context menu (new file/dir, rename, delete), drag & drop file moving. Configurable visibility for gitignored files and .git directory (visible/grayed/hidden). Instant git status updates via QFileSystemWatcher + 10s polling. Works over SSH via sshfs
- **Code Editor** — Tabbed editor with syntax highlighting for C/C++, Python, JavaScript/TypeScript, Rust. Line numbers, Ctrl+S save. Split view (horizontal/vertical). Find & Replace (Ctrl+F/Ctrl+H) with yellow match highlighting and scrollbar markers. Undo/Redo. Unsaved changes tracking with `●` tab marker. Configurable current line highlighting
- **Dual Terminals** — AI-terminal (top, sends prompts) + general Terminal (bottom tab). Both follow file browser directory and support SSH. Stop button sends configurable stop sequence (default: Ctrl+C)
- **Prompt System** — Prompt input with configurable send key (Enter / Ctrl+Enter). Shift modifier = send + save. Saved/recurring prompts per project
- **Project Management** — `.LLM/instructions.json` stores project metadata, numbered prompt history, and saved prompt IDs. Auto-creates `.gitignore`
- **Markdown Preview** — Ctrl+M opens live preview for `.md` files. Multiple previews can be open simultaneously (one per .md file). Markdown tab icon (👁) for quick toggle. Syntax-highlighted code blocks via bundled highlight.js (46+ languages including assembly, VHDL, GLSL, LaTeX, etc.). KaTeX math rendering ($$...$$ and $...$). Mermaid diagram rendering. Dark/light themes (VS2015/VS style for code). Optional libcmark for conversion with regex fallback. 500ms debounced live updates. Context-dependent zoom (Ctrl+=/Ctrl+- affects only the focused component). **PDF Export** via Command Palette — renders preview to PDF with configurable margins, orientation, page numbering, and optional page border
- **Git Integration** — Commit button in Git tab with editable commit message dialog (auto-init + add + commit). Auto-filters sensitive files (.env, *.pem, *.key). Async git status with instant updates for file browser colors. **Git Graph tab** with visual commit history, branch/merge visualization, Fetch/Pull/Push, upstream tracking, git user info editing, and multi-remote management
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
- **Command Palette** — Ctrl+Shift+P for fuzzy-searchable commands (split view, focus, themes, diff refresh)
- **Notifications** — Centralized log with Info/Warning/Error/Success levels and unread badge
- **Custom Title Bar** — VS Code/Zed-style frameless window (CSD) with themed minimize/maximize/close buttons. All dialogs use themed title bars
- **Global Themes** — Unified theme system: Dark, Dark Soft, Dark Warm, Light, Monokai, Solarized Dark, Solarized Light, Nord. Auto-imports installed Zed editor themes. Live switching without restart
- **Settings** — Tabbed dialog with configurable fonts, sizes, and global theme for all components (terminal, editor, file browser, prompt, diff viewer, changes monitor, visibility, PDF export)
- **Session Persistence** — Remembers window size, splitter positions, open files, active tab. Multi-monitor aware
- **Fully Offline** — All resources (mermaid.js, KaTeX, highlight.js, fonts) are bundled with integrity verification on startup. No network requests. WebEngine remote URL access explicitly disabled

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
```

> **Note:** `sshfs` is required for SSH remote file browsing and transfers. `qt6-webengine-dev` requires `qt6-webchannel-dev` as a dependency on some Debian versions. `qt6-pdf-dev` is required for PDF export with page numbering. On older Debian/Ubuntu the qtermwidget package may be named `libqtermwidget6-1-dev` instead of `libqtermwidget-dev`. Optional: install `libcmark-dev` for better markdown conversion (loaded at runtime via dlopen).

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
11. **Command Palette** — Ctrl+Shift+P to search and execute commands (split view, focus panels, switch themes)
12. **Changes Monitor** — Bottom "Changes" tab shows real-time file modifications with diff preview and revert
13. **Git Graph** — Bottom "Git" tab shows commit history as a visual graph. Use Fetch/Pull/Push buttons, manage remotes via "Remotes" button
14. **Settings** — Hamburger menu (☰) → Settings. Tabbed dialog for fonts, themes, and behavior
15. **Zed Themes** — Install themes in Zed editor and they automatically appear in Settings and command palette
16. **Markdown Preview** — Open a `.md` file and press Ctrl+M (or click the 👁 icon on the tab). Multiple previews can be open at once. Press Ctrl+M again to close the current preview
17. **Export to PDF** — With preview open, use Command Palette (Ctrl+Shift+P) → "Export Preview to PDF". Configure margins, orientation, page numbering, and border in Settings > PDF
18. **Git User** — Click "User" in Git tab to view/edit git global name and email
19. **Zoom** — Ctrl+= / Ctrl+- zooms only the focused component (editor, prompt, file browser, or markdown preview)

## Documentation

- [Architecture](docs/architecture.md) — Component design and data flow
- [Diagrams](docs/diagrams.md) — Mermaid diagrams of class hierarchy, signal flow, and async pipelines
- [Dependencies](docs/dependencies.md) — Build and runtime dependencies with Debian 13 package versions
- [Changelog](docs/changelog.md) — Version history

## Author

**krse**

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
