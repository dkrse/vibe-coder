# Vibe Coder

A Qt6 C++ IDE-like application for AI-assisted development workflows. Combines a Zed-style file browser, code editor with syntax highlighting, dual embedded terminals, SSH remote development, and prompt system in a single window.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt6](https://img.shields.io/badge/Qt-6-green)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

## Features

- **File Browser** — Zed editor-style tree view with git status colors (modified, untracked, added), dark/light theme, context menu (new file/dir, rename, delete). Works over SSH via sshfs
- **Code Editor** — Tabbed editor with syntax highlighting for C/C++, Python, JavaScript/TypeScript, Rust. Line numbers, dark/light scheme, Ctrl+S save
- **Dual Terminals** — AI-terminal (top, sends prompts) + general Terminal (bottom tab). Both follow file browser directory and support SSH
- **Prompt System** — Prompt input with configurable send key (Enter / Ctrl+Enter). Shift modifier = send + save. Saved/recurring prompts per project
- **Project Management** — `.LLM/instructions.json` stores project metadata, numbered prompt history, and saved prompt IDs. Auto-creates `.gitignore`
- **Git Integration** — Async one-click commit (auto-init + add + commit). Auto-filters sensitive files (.env, *.pem, *.key). Async git status polling for file browser colors
- **SSH Remote Development**
  - Multiple simultaneous SSH profiles with profile switcher
  - sshfs-based file browsing (synchronous QStandardItemModel for reliability)
  - Auto-reconnect on connection loss (health check every 5s, up to 3 retries)
  - File upload/download via sshfs mount with rsync progress bar
  - SSH port forwarding management (local -L / remote -R tunnels)
  - Saved connections (passwords never persisted)
  - Terminals auto-cd when opening remote files
- **Settings** — Configurable fonts, sizes, color schemes, and themes for all components
- **Session Persistence** — Remembers window size, splitter positions, open files, and active tab

## Installation

### Debian / Ubuntu

```bash
# Install dependencies
sudo apt update
sudo apt install -y \
    build-essential cmake \
    qt6-base-dev \
    libqtermwidget6-1-dev \
    libutf8proc-dev \
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

> **Note:** `sshfs` is required for SSH remote file browsing and transfers. If `libutf8proc-dev` is unavailable, the build links `libqtermwidget6.so.2` directly (see CMakeLists.txt).

### Fedora

```bash
sudo dnf install -y \
    gcc-c++ cmake \
    qt6-qtbase-devel \
    qtermwidget-devel \
    utf8proc-devel \
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
    qtermwidget \
    utf8proc \
    sshfs

git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

1. **Open a directory** — Use the path bar or `...` button in the file browser
2. **Edit files** — Click a file to open it in a new tab. Ctrl+S to save
3. **Use the terminal** — AI-terminal (top) for prompts, Terminal tab (bottom) for general use
4. **Send prompts** — Type in the prompt area, press Ctrl+Enter (or Enter, configurable in Settings)
5. **Save prompts** — Shift+Enter (or Ctrl+Shift+Enter) sends and saves prompt for reuse
6. **Create a project** — Hamburger menu (☰) → Create Project. Stores config in `.LLM/instructions.json`
7. **SSH Connect** — Hamburger menu (☰) → SSH Connect. Enter user/host/password or select saved connection
8. **SSH Tunnels** — Hamburger menu (☰) → SSH Tunnels. Create local (-L) or remote (-R) port forwards
9. **Upload/Download** — Hamburger menu (☰) → SSH Upload/Download. Transfers via sshfs mount with progress
10. **Commit** — Click Commit button. Auto-initializes git if needed, stages files (filters sensitive), commits with timestamp
11. **Settings** — Hamburger menu (☰) → Settings. Configure fonts, themes, and behavior for all components

## Documentation

- [Architecture](docs/architecture.md) — Component design and data flow
- [Diagrams](docs/diagrams.md) — Mermaid diagrams of class hierarchy, signal flow, and async pipelines
- [Changelog](docs/changelog.md) — Version history

## Author

**krse**

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
