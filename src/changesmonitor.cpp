#include "changesmonitor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QFont>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

// ── DiffBlockHighlighter ────────────────────────────────────────────

DiffBlockHighlighter::DiffBlockHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void DiffBlockHighlighter::highlightBlock(const QString &text)
{
    QTextCharFormat fmt;
    if (text.startsWith("+++") || text.startsWith("---")) {
        fmt.setForeground(QColor("#569cd6"));
        fmt.setFontWeight(QFont::Bold);
    } else if (text.startsWith("@@")) {
        fmt.setForeground(QColor("#c586c0"));
    } else if (text.startsWith("+")) {
        fmt.setForeground(QColor("#4ec9b0"));
        fmt.setBackground(QColor("#1e3a1e"));
    } else if (text.startsWith("-")) {
        fmt.setForeground(QColor("#f44747"));
        fmt.setBackground(QColor("#3a1e1e"));
    } else {
        return;
    }
    setFormat(0, text.length(), fmt);
}

// ── ChangesMonitor ──────────────────────────────────────────────────

ChangesMonitor::ChangesMonitor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    m_infoLabel = new QLabel("No changes detected");
    m_revertBtn = new QPushButton("Revert");
    m_revertBtn->setToolTip("Revert selected file to last git version");
    m_revertBtn->setEnabled(false);
    m_openBtn = new QPushButton("Open");
    m_openBtn->setToolTip("Open selected file in editor");
    m_openBtn->setEnabled(false);
    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setFixedWidth(70);
    m_clearBtn = new QPushButton("Clear");
    m_clearBtn->setFixedWidth(60);

    toolbar->addWidget(m_infoLabel, 1);
    toolbar->addWidget(m_openBtn);
    toolbar->addWidget(m_revertBtn);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addWidget(m_clearBtn);
    layout->addLayout(toolbar);

    // Split: file list | diff preview
    auto *splitter = new QSplitter(Qt::Horizontal);

    m_fileList = new QListWidget;
    m_fileList->setMaximumWidth(300);
    m_fileList->setStyleSheet(
        "QListWidget { font-size: 12px; background-color: #252526; color: #d4d4d4; }"
        "QListWidget::item { padding: 4px 6px; }"
        "QListWidget::item:selected { background-color: #094771; }");
    splitter->addWidget(m_fileList);

    m_diffPreview = new QPlainTextEdit;
    m_diffPreview->setReadOnly(true);
    m_diffPreview->setFont(QFont("Monospace", 10));
    m_diffPreview->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diffPreview->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    m_diffHighlighter = new DiffBlockHighlighter(m_diffPreview->document());
    splitter->addWidget(m_diffPreview);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter, 1);

    // Watcher
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &ChangesMonitor::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &ChangesMonitor::onDirChanged);

    // Periodic scan for new files (watcher doesn't catch new files)
    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, &ChangesMonitor::scanForNewFiles);
    m_scanTimer->start(3000);

    // Connections
    connect(m_fileList, &QListWidget::currentRowChanged, this, [this](int row) {
        bool valid = row >= 0 && row < m_changes.size();
        m_revertBtn->setEnabled(valid && !m_changes[row].reverted);
        m_openBtn->setEnabled(valid);
        if (valid)
            showDiffForFile(m_changes[row].filePath);
        else
            m_diffPreview->clear();
    });

    connect(m_revertBtn, &QPushButton::clicked, this, &ChangesMonitor::revertSelected);

    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        int row = m_fileList->currentRow();
        if (row >= 0 && row < m_changes.size())
            emit openFileRequested(m_changes[row].filePath);
    });

    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        if (!m_projectDir.isEmpty())
            setProjectDir(m_projectDir);
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_changes.clear();
        refreshList();
        m_diffPreview->clear();
        m_infoLabel->setText("No changes detected");
    });
}

void ChangesMonitor::setProjectDir(const QString &dir)
{
    // Remove old watches
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    m_knownFiles.clear();

    m_projectDir = dir;
    if (dir.isEmpty()) return;

    // Watch the project directory
    m_watcher->addPath(dir);

    // Scan existing files and record mtimes
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QStringList watchPaths;
    while (it.hasNext()) {
        it.next();
        QString path = it.filePath();

        // Skip hidden dirs (.git, .LLM, etc.)
        if (path.contains("/.git/") || path.contains("/.LLM/") ||
            path.contains("/node_modules/") || path.contains("/__pycache__/"))
            continue;

        QFileInfo fi(path);
        m_knownFiles[path] = fi.lastModified();
        watchPaths.append(path);

        if (watchPaths.size() >= 2000) break; // limit
    }

    if (!watchPaths.isEmpty())
        m_watcher->addPaths(watchPaths);

    // Also watch subdirectories for new file detection
    QDirIterator dit(dir, QDir::Dirs | QDir::NoDotAndDotDot,
                     QDirIterator::Subdirectories);
    QStringList dirPaths;
    while (dit.hasNext()) {
        dit.next();
        QString dpath = dit.filePath();
        if (dpath.contains("/.git") || dpath.contains("/.LLM") ||
            dpath.contains("/node_modules") || dpath.contains("/__pycache__"))
            continue;
        dirPaths.append(dpath);
        if (dirPaths.size() >= 500) break;
    }
    if (!dirPaths.isEmpty())
        m_watcher->addPaths(dirPaths);
}

void ChangesMonitor::onFileChanged(const QString &path)
{
    // Skip if already recorded recently (within 1s)
    for (const auto &c : m_changes) {
        if (c.filePath == path && c.timestamp.secsTo(QDateTime::currentDateTime()) < 1)
            return;
    }

    // Check if file actually changed (mtime differs)
    QFileInfo fi(path);
    if (!fi.exists()) {
        // File was deleted — record as change
    } else {
        QDateTime lastKnown = m_knownFiles.value(path);
        if (lastKnown.isValid() && fi.lastModified() == lastKnown)
            return;
        m_knownFiles[path] = fi.lastModified();
    }

    // Remove duplicate if same file already in list
    for (int i = m_changes.size() - 1; i >= 0; --i) {
        if (m_changes[i].filePath == path) {
            m_changes.removeAt(i);
            break;
        }
    }

    FileChange change;
    change.filePath = path;
    change.timestamp = QDateTime::currentDateTime();
    m_changes.prepend(change); // newest first

    refreshList();
    m_infoLabel->setText(QString("%1 file(s) changed").arg(m_changes.size()));

    emit changeDetected(path);

    // Re-add to watcher (QFileSystemWatcher removes after signal on some platforms)
    if (fi.exists() && !m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void ChangesMonitor::onDirChanged(const QString &)
{
    scanForNewFiles();
}

void ChangesMonitor::scanForNewFiles()
{
    if (m_projectDir.isEmpty()) return;

    QDirIterator it(m_projectDir, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString path = it.filePath();

        if (path.contains("/.git/") || path.contains("/.LLM/") ||
            path.contains("/node_modules/") || path.contains("/__pycache__/"))
            continue;

        if (!m_knownFiles.contains(path)) {
            // New file detected
            QFileInfo fi(path);
            m_knownFiles[path] = fi.lastModified();

            if (!m_watcher->files().contains(path))
                m_watcher->addPath(path);

            // Record as change
            FileChange change;
            change.filePath = path;
            change.timestamp = QDateTime::currentDateTime();
            m_changes.prepend(change);

            refreshList();
            m_infoLabel->setText(QString("%1 file(s) changed").arg(m_changes.size()));
            emit changeDetected(path);
        }
    }
}

void ChangesMonitor::showDiffForFile(const QString &filePath)
{
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_projectDir);

    // Get relative path
    QString relPath = filePath;
    if (relPath.startsWith(m_projectDir))
        relPath = relPath.mid(m_projectDir.length() + 1);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, filePath](int, QProcess::ExitStatus) {
        QString output = proc->readAllStandardOutput();
        if (output.trimmed().isEmpty()) {
            // Maybe untracked — show file content
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                output = "(new file)\n" + f.readAll();
            } else {
                output = "(no diff available)";
            }
        }
        m_diffPreview->setPlainText(output);
        proc->deleteLater();
    });

    proc->start("git", {"diff", "--no-color", "--", relPath});
}

void ChangesMonitor::revertSelected()
{
    int row = m_fileList->currentRow();
    if (row < 0 || row >= m_changes.size()) return;

    FileChange &change = m_changes[row];
    if (change.reverted) return;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_projectDir);

    QString relPath = change.filePath;
    if (relPath.startsWith(m_projectDir))
        relPath = relPath.mid(m_projectDir.length() + 1);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, row, relPath](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            m_changes[row].reverted = true;
            refreshList();
            m_diffPreview->setPlainText("(reverted to git version)");
            emit fileReverted(m_changes[row].filePath);
        } else {
            m_diffPreview->setPlainText("Revert failed:\n" + proc->readAllStandardError());
        }
        proc->deleteLater();
    });

    proc->start("git", {"checkout", "--", relPath});
}

void ChangesMonitor::setViewerFont(const QFont &font)
{
    m_diffPreview->setFont(font);
}

void ChangesMonitor::setViewerColors(const QColor &bg, const QColor &fg)
{
    m_diffPreview->setStyleSheet(
        QString("QPlainTextEdit { background-color: %1; color: %2; }")
            .arg(bg.name(), fg.name()));
    m_fileList->setStyleSheet(
        QString("QListWidget { font-size: 12px; background-color: %1; color: %2; }"
                "QListWidget::item { padding: 4px 6px; }"
                "QListWidget::item:selected { background-color: #094771; }")
            .arg(bg.name(), fg.name()));
}

void ChangesMonitor::refreshList()
{
    int prevRow = m_fileList->currentRow();
    m_fileList->clear();

    for (const auto &change : m_changes) {
        QString relPath = change.filePath;
        if (relPath.startsWith(m_projectDir))
            relPath = relPath.mid(m_projectDir.length() + 1);

        QString time = change.timestamp.toString("hh:mm:ss");
        QString label = QString("[%1] %2").arg(time, relPath);
        if (change.reverted)
            label += "  (reverted)";

        auto *item = new QListWidgetItem(label);
        if (change.reverted)
            item->setForeground(QColor("#666666"));
        else if (!QFile::exists(change.filePath))
            item->setForeground(QColor("#f44747")); // deleted
        else
            item->setForeground(QColor("#4ec9b0")); // modified/new
        m_fileList->addItem(item);
    }

    if (prevRow >= 0 && prevRow < m_fileList->count())
        m_fileList->setCurrentRow(prevRow);

    m_infoLabel->setText(m_changes.isEmpty()
        ? "No changes detected"
        : QString("%1 file(s) changed").arg(m_changes.size()));
}
