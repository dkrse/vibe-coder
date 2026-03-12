# Vibe Coder

A Qt6 C++ IDE-like application for AI-assisted development workflows. Combines a Zed-style file browser, code editor with syntax highlighting, embedded terminal, and prompt system in a single window.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)
![Qt6](https://img.shields.io/badge/Qt-6-green)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)

## Features

- **File Browser** — Zed editor-style tree view with git status colors (modified, untracked, added), dark/light theme, context menu (new file/dir, rename, delete)
- **Code Editor** — Tabbed editor with syntax highlighting for C/C++, Python, JavaScript/TypeScript, Rust. Line numbers, dark/light scheme, Ctrl+S save
- **Terminal** — Embedded QTermWidget terminal emulator. Runs interactive apps (claude, mc, vim). Follows file browser directory
- **Prompt System** — Prompt input with configurable send key (Enter / Ctrl+Enter). Sends text directly to terminal. Logs all prompts to project file
- **Project Management** — `.LLM/instructions.json` stores project metadata and numbered prompt history. Auto-creates `.gitignore`
- **Git Integration** — One-click commit (auto-init + add + commit). Async git status polling for file browser colors
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
    libutf8proc-dev

# Clone and build
git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./vibe-coder
```

> **Note:** If `libutf8proc-dev` is unavailable, the build links `libqtermwidget6.so.2` directly (see CMakeLists.txt).

### Fedora

```bash
# Install dependencies
sudo dnf install -y \
    gcc-c++ cmake \
    qt6-qtbase-devel \
    qtermwidget-devel \
    utf8proc-devel

# Clone and build
git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./vibe-coder
```

### Arch Linux

```bash
sudo pacman -S --needed \
    base-devel cmake \
    qt6-base \
    qtermwidget \
    utf8proc

git clone https://github.com/krse/vibe-coder.git
cd vibe-coder
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

1. **Open a directory** — Use the path bar or `...` button in the file browser
2. **Edit files** — Click a file to open it in a new tab. Ctrl+S to save
3. **Use the terminal** — Terminal tab is always available. Supports all interactive CLI apps
4. **Send prompts** — Type in the prompt area, press Ctrl+Enter (or Enter, configurable in Settings)
5. **Create a project** — Hamburger menu (☰) → Create Project. Stores config in `.LLM/instructions.json`
6. **Commit** — Click Commit button. Auto-initializes git if needed, stages all files, commits with timestamp
7. **Settings** — Hamburger menu (☰) → Settings. Configure fonts, themes, and behavior for all components

## Documentation

- [Architecture](docs/architecture.md) — Component design and data flow
- [Diagrams](docs/diagrams.md) — Mermaid diagrams of class hierarchy, signal flow, and async pipelines
- [Changelog](docs/changelog.md) — Version history

## Author

**krse**

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
