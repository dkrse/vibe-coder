# Vibe Coder - Dependencies

Tested on **Debian GNU/Linux 13 (trixie)** amd64 and **Fedora Linux 43 (Workstation Edition)** x86_64.

## Build Tools

| Tool    | Debian 13 | Fedora 43 | Debian package     | Fedora package |
|---------|-----------|-----------|--------------------|--------------------|
| CMake   | 3.31.6    | 3.31.10   | `cmake`            | `cmake`            |
| g++     | 14.2.0    | 15.2.1    | `g++-14`           | `gcc-c++`          |
| Git     | 2.47.3    | 2.53.0    | `git`              | `git`              |

## Required (build)

| Library / Module         | Debian 13              | Fedora 43 | Debian package              | Fedora package              |
|--------------------------|------------------------|-----------|-----------------------------|-----------------------------|
| Qt6 Core                 | 6.8.2+dfsg-9+deb13u1  | 6.10.2    | `qt6-base-dev`              | `qt6-qtbase-devel`          |
| Qt6 Gui                  | 6.8.2+dfsg-9+deb13u1  | 6.10.2    | `qt6-base-dev`              | `qt6-qtbase-devel`          |
| Qt6 Widgets              | 6.8.2+dfsg-9+deb13u1  | 6.10.2    | `qt6-base-dev`              | `qt6-qtbase-devel`          |
| Qt6 WebEngineWidgets     | 6.8.2+dfsg-4           | 6.10.2    | `qt6-webengine-dev`         | `qt6-qtwebengine-devel`     |
| Qt6 PdfWidgets           | 6.8.2+dfsg-4           | 6.10.2    | `qt6-pdf-dev`               | `qt6-qtpdf-devel`           |
| qtermwidget6              | 2.1.0                  | 2.3.0     | `libqtermwidget-dev`        | `qtermwidget-devel`         |
| cmark-gfm                | 0.29.0.gfm.13          | (same)    | (CMake FetchContent)        | (CMake FetchContent)        |

### Install (build)

**Debian / Ubuntu:**
```bash
sudo apt install cmake g++ git \
    qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
    qt6-webengine-dev qt6-webchannel-dev qt6-pdf-dev \
    qt6-positioning-dev qt6-declarative-dev \
    libqtermwidget-dev
```

**Fedora:**
```bash
sudo dnf install cmake gcc-c++ git \
    qt6-qtbase-devel \
    qt6-qtwebengine-devel \
    qt6-qtpdf-devel \
    qtermwidget-devel
```

## Required (runtime)

| Dependency               | Debian 13 | Fedora 43 | Debian package              | Fedora package    | Purpose                        |
|--------------------------|-----------|-----------|-----------------------------|--------------------|--------------------------------|
| Qt6 Core/Gui/Widgets     | 6.8.2     | 6.10.2    | `libqt6core6t64` etc.       | `qt6-qtbase`       | UI framework                   |
| Qt6 WebEngine            | 6.8.2     | 6.10.2    | `libqt6webenginewidgets6`   | `qt6-qtwebengine`  | Markdown preview               |
| Qt6 PdfWidgets           | 6.8.2     | 6.10.2    | `libqt6pdfwidgets6`         | `qt6-qtpdf`        | PDF export                     |
| qtermwidget6              | 2.1.0     | 2.3.0     | `libqtermwidget6-2`         | `qtermwidget`      | Embedded terminal              |
| sshfs                    | 3.7.3     | 3.7.5     | `sshfs`                     | `fuse-sshfs`       | SSH remote filesystem mount    |
| git                      | 2.47.3    | 2.53.0    | `git`                       | `git`              | Git integration (status, log)  |

### Install (runtime)

**Debian / Ubuntu:**
```bash
sudo apt install sshfs git
```

**Fedora:**
```bash
sudo dnf install fuse-sshfs git
```

## Optional (runtime)

| Dependency  | Debian 13 | Fedora 43 | Debian package       | Fedora package | Purpose                                              |
|-------------|-----------|-----------|----------------------|----------------|------------------------------------------------------|
| rsync       | 3.4.1     | 3.4.1     | `rsync`              | `rsync`        | Efficient SSH file transfer (falls back to cp)       |

### Widget styles (optional)

| Style      | Debian package             | Fedora package             | Description                        |
|------------|----------------------------|----------------------------|------------------------------------|
| Adwaita    | `adwaita-qt6`              | —                          | GNOME Adwaita look                 |
| Breeze     | `kde-style-breeze-data`    | `plasma-breeze-qt5`        | KDE Plasma Breeze                  |
| Oxygen     | `kde-style-oxygen-qt6`     | —                          | Classic KDE Oxygen                 |
| Kvantum    | `qt6-style-kvantum`        | `kvantum`                  | SVG-based engine with many themes  |

> Fusion and Windows styles are always available (built into Qt6).

### Install (optional)

**Debian / Ubuntu:**
```bash
sudo apt install rsync adwaita-qt6 kde-style-breeze-data qt6-style-kvantum kde-style-oxygen-qt6
```

**Fedora:**
```bash
sudo dnf install rsync kvantum
```

## Themes

Themes are loaded from `~/.config/vibe-coder/themes/` at startup. The repository ships with 8 themes in the `themes/` directory. Copy them after building:

```bash
mkdir -p ~/.config/vibe-coder/themes
cp themes/*.json ~/.config/vibe-coder/themes/
```

Supported formats (auto-detected):
- **Native** — flat JSON with `background`, `foreground`, `altBackground`, `border`, `hover`, `selected`, `lineHighlight`, `terminalScheme`
- **Zed** — Zed editor theme format (`themes[]` array with `style` object)
- **VS Code** — VS Code theme format (`colors` object with `editor.background`, etc.)

## Statically Linked (fetched at build time via CMake FetchContent)

| Library          | Version          | Purpose                                    |
|------------------|------------------|--------------------------------------------|
| cmark-gfm        | 0.29.0.gfm.13   | GitHub Flavored Markdown parsing (tables, strikethrough, autolinks) |

> cmark-gfm is downloaded from GitHub during `cmake` configure and statically linked. No runtime dependency.

## Bundled Assets (compiled into binary via Qt resources)

| Asset               | Purpose                           |
|---------------------|-----------------------------------|
| mermaid.min.js      | Diagram rendering in preview      |
| highlight.min.js    | Code syntax highlighting          |
| katex.min.js        | Math notation rendering           |
| auto-render.min.js  | KaTeX auto-render                 |
| KaTeX WOFF2 fonts   | Math font rendering               |

## Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```
