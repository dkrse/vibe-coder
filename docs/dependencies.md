# Vibe Coder - Dependencies

Tested on **Debian GNU/Linux 13 (trixie)**, amd64.

## Build Tools

| Tool    | Version  | Debian package     |
|---------|----------|--------------------|
| CMake   | 3.31.6   | `cmake`            |
| g++     | 14.2.0   | `g++-14`           |
| Git     | 2.47.3   | `git`              |

## Required (build)

| Library / Module         | Version                | Debian package              |
|--------------------------|------------------------|-----------------------------|
| Qt6 Core                 | 6.8.2+dfsg-9+deb13u1  | `qt6-base-dev`              |
| Qt6 Gui                  | 6.8.2+dfsg-9+deb13u1  | `qt6-base-dev`              |
| Qt6 Widgets              | 6.8.2+dfsg-9+deb13u1  | `qt6-base-dev`              |
| Qt6 WebEngineWidgets     | 6.8.2+dfsg-4           | `qt6-webengine-dev`         |
| Qt6 PdfWidgets           | 6.8.2+dfsg-4           | `qt6-pdf-dev`               |
| qtermwidget6              | 2.1.0                  | `libqtermwidget-dev`        |
| libdl (dlopen)           | glibc                  | `libc6-dev`                 |

### Install (build)

```bash
sudo apt install cmake g++ git \
    qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
    qt6-webengine-dev qt6-webchannel-dev qt6-pdf-dev \
    qt6-positioning-dev qt6-declarative-dev \
    libqtermwidget-dev
```

## Required (runtime)

| Dependency               | Version  | Debian package              | Purpose                        |
|--------------------------|----------|-----------------------------|--------------------------------|
| Qt6 Core/Gui/Widgets     | 6.8.2    | `libqt6core6t64` etc.       | UI framework                   |
| Qt6 WebEngine            | 6.8.2    | `libqt6webenginewidgets6`   | Markdown preview               |
| Qt6 PdfWidgets           | 6.8.2    | `libqt6pdfwidgets6`         | PDF export                     |
| qtermwidget6              | 2.1.0    | `libqtermwidget6-2`         | Embedded terminal              |
| sshfs                    | 3.7.3    | `sshfs`                     | SSH remote filesystem mount    |
| git                      | 2.47.3   | `git`                       | Git integration (status, log)  |

### Install (runtime)

```bash
sudo apt install sshfs git
```

## Optional (runtime)

| Dependency  | Version  | Debian package       | Purpose                                              |
|-------------|----------|----------------------|------------------------------------------------------|
| libcmark    | 0.30.2   | `libcmark0.30.2`     | Fast markdown-to-HTML (loaded via dlopen, regex fallback) |
| rsync       | 3.4.1    | `rsync`              | Efficient SSH file transfer (falls back to cp)       |

### Widget styles (optional)

| Style      | Debian package             | Description                        |
|------------|----------------------------|------------------------------------|
| Adwaita    | `adwaita-qt6`              | GNOME Adwaita look                 |
| Breeze     | `kde-style-breeze-data`    | KDE Plasma Breeze                  |
| Oxygen     | `kde-style-oxygen-qt6`     | Classic KDE Oxygen                 |
| Kvantum    | `qt6-style-kvantum`        | SVG-based engine with many themes  |

> Fusion and Windows styles are always available (built into Qt6).

### Install (optional)

```bash
sudo apt install libcmark0.30.2 rsync adwaita-qt6 kde-style-breeze-data qt6-style-kvantum kde-style-oxygen-qt6
```

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
