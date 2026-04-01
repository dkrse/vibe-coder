#include "filebrowser.h"
#include "sshmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include "themeddialog.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

// ── FileBrowserTreeView (drag & drop) ───────────────────────────────

FileBrowserTreeView::FileBrowserTreeView(FileBrowser *fb, QWidget *parent)
    : QTreeView(parent), m_fb(fb)
{
}

void FileBrowserTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->source() == this)
        event->acceptProposedAction();
    else
        QTreeView::dragEnterEvent(event);
}

void FileBrowserTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    QModelIndex idx = indexAt(event->position().toPoint());
    if (idx.isValid() && m_fb->isDir(idx)) {
        event->acceptProposedAction();
        setDropIndicatorShown(true);
    } else if (!idx.isValid()) {
        // dropping on root
        event->acceptProposedAction();
    } else {
        // dropping on a file — target is its parent dir
        event->acceptProposedAction();
    }
    QTreeView::dragMoveEvent(event);
}

void FileBrowserTreeView::dropEvent(QDropEvent *event)
{
    QModelIndex idx = indexAt(event->position().toPoint());
    QString targetDir;

    if (idx.isValid()) {
        if (m_fb->isDir(idx))
            targetDir = m_fb->filePath(idx);
        else
            targetDir = QFileInfo(m_fb->filePath(idx)).absolutePath();
    } else {
        targetDir = m_fb->rootPath();
    }

    // Get source paths from selection
    QModelIndex srcIdx = currentIndex();
    if (!srcIdx.isValid()) {
        event->ignore();
        return;
    }

    QString srcPath = m_fb->filePath(srcIdx);
    if (srcPath.isEmpty() || srcPath == targetDir) {
        event->ignore();
        return;
    }

    QString srcName = QFileInfo(srcPath).fileName();
    QString destPath = targetDir + "/" + srcName;

    // Don't move into itself
    if (destPath == srcPath || targetDir.startsWith(srcPath + "/")) {
        event->ignore();
        return;
    }

    // Check if destination already exists
    if (QFileInfo::exists(destPath)) {
        event->ignore();
        return;
    }

    // Move file or directory
    bool ok = QFile::rename(srcPath, destPath);
    if (ok) {
        event->acceptProposedAction();
        if (m_fb->isSshActive())
            m_fb->setRootPath(m_fb->rootPath()); // refresh SSH model
    } else {
        event->ignore();
    }
}

// ── FileItemDelegate (Zed-style + git colors) ──────────────────────

void FileItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    painter->save();

    QString name = index.data(Qt::DisplayRole).toString();
    QString fullPath;
    bool isDir = false;

    // Get path and isDir from whichever model is active
    if (m_fb) {
        fullPath = m_fb->filePath(index);
        isDir = m_fb->isDir(index);
    }

    bool dark = m_fb && m_fb->isDark();

    // Background
    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, m_lineHighlight.isValid() ? m_lineHighlight : (dark ? QColor("#2a2d2e") : QColor("#e8e8e8")));
    else if (option.state & QStyle::State_MouseOver)
        painter->fillRect(option.rect, m_hoverBg.isValid() ? m_hoverBg : (dark ? QColor("#2a2d2e") : QColor("#e8e8e8")));

    QRect textRect = option.rect;
    textRect.setLeft(textRect.left() + 4);

    // Determine color
    QColor textColor;
    QString icon;

    // Check .git directory visibility (grayed)
    bool gitColored = false;
    if (m_fb && !fullPath.isEmpty()) {
        QString baseName = QFileInfo(fullPath).fileName();
        if (baseName == ".git" && m_fb->dotGitVisibility() == "grayed") {
            textColor = dark ? QColor("#5a5a5a") : QColor("#b0b0b0");
            gitColored = true;
        }
    }

    // Check git status
    if (!gitColored && m_fb && m_fb->hasGit() && !fullPath.isEmpty()) {
        auto status = m_fb->gitStatus(fullPath);
        if (status == FileBrowser::Ignored && m_fb->gitignoreVisibility() != "visible") {
            textColor = dark ? QColor("#5a5a5a") : QColor("#b0b0b0");
            gitColored = true;
        } else if (status == FileBrowser::Modified) {
            textColor = dark ? QColor("#e2c08d") : QColor("#986801");
            gitColored = true;
        } else if (status == FileBrowser::Untracked) {
            textColor = dark ? QColor("#73c991") : QColor("#22863a");
            gitColored = true;
        } else if (status == FileBrowser::Added) {
            textColor = dark ? QColor("#73c991") : QColor("#22863a");
            gitColored = true;
        }
    }

    if (isDir) {
        bool expanded = option.state & QStyle::State_Open;
        icon = expanded ? "\u25BE " : "\u25B8 ";
        if (!gitColored)
            textColor = dark ? QColor("#c5c5c5") : QColor("#333333");
    } else {
        icon = "  ";
        if (!gitColored) {
            QString suffix = QFileInfo(name).suffix().toLower();
            if (suffix == "cpp" || suffix == "c" || suffix == "h" || suffix == "hpp")
                textColor = dark ? QColor("#519aba") : QColor("#0550ae");
            else if (suffix == "py")
                textColor = dark ? QColor("#4584b6") : QColor("#0550ae");
            else if (suffix == "js" || suffix == "ts" || suffix == "jsx" || suffix == "tsx")
                textColor = dark ? QColor("#e8d44d") : QColor("#953800");
            else if (suffix == "rs")
                textColor = dark ? QColor("#dea584") : QColor("#953800");
            else if (suffix == "json")
                textColor = dark ? QColor("#a9dc76") : QColor("#116329");
            else if (suffix == "md" || suffix == "txt")
                textColor = dark ? QColor("#c5c5c5") : QColor("#57606a");
            else
                textColor = dark ? QColor("#d4d4d4") : QColor("#24292f");
        }
    }

    painter->setPen(textColor);
    painter->setFont(option.font);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, icon + name);

    painter->restore();
}

QSize FileItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(s.height(), option.fontMetrics.height() + 4));
    return s;
}

// ── FileBrowserProxy ────────────────────────────────────────────────

FileBrowserProxy::FileBrowserProxy(FileBrowser *fb, QObject *parent)
    : QSortFilterProxyModel(parent), m_fb(fb)
{
}

void FileBrowserProxy::refresh()
{
    invalidateFilter();
}

void FileBrowserProxy::notifyAllChanged()
{
    // Force complete re-layout so delegate repaints all visible items
    emit layoutChanged();
}

bool FileBrowserProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel) return QSortFilterProxyModel::lessThan(left, right);

    bool leftDir = fsModel->isDir(left);
    bool rightDir = fsModel->isDir(right);

    // Directories always come before files
    if (leftDir != rightDir)
        return leftDir;

    // Same type: compare names case-insensitively
    return fsModel->fileName(left).compare(fsModel->fileName(right), Qt::CaseInsensitive) < 0;
}

bool FileBrowserProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    auto *fsModel = qobject_cast<QFileSystemModel *>(sourceModel());
    if (!fsModel) return true;

    QModelIndex idx = fsModel->index(sourceRow, 0, sourceParent);
    QString name = fsModel->fileName(idx);
    QString fullPath = fsModel->filePath(idx);

    // .git directory
    if (name == ".git" && m_fb->dotGitVisibility() == "hidden")
        return false;

    // Gitignored files
    if (m_fb->gitignoreVisibility() == "hidden" && m_fb->hasGit()) {
        if (m_fb->gitStatus(fullPath) == FileBrowser::Ignored)
            return false;
    }

    return true;
}

// ── FileBrowser ─────────────────────────────────────────────────────

FileBrowser::FileBrowser(QWidget *parent)
    : QWidget(parent)
{
    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setReadOnly(false);
    m_fsModel->setFilter(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

    m_proxyModel = new FileBrowserProxy(this, this);
    m_proxyModel->setSourceModel(m_fsModel);
    m_proxyModel->setDynamicSortFilter(true);

    m_sshModel = new QStandardItemModel(this);

    m_delegate = new FileItemDelegate(this);
    m_delegate->setFileBrowser(this);

    m_treeView = new FileBrowserTreeView(this, this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setItemDelegate(m_delegate);
    m_treeView->setAnimated(false);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    m_treeView->header()->hide();
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setIndentation(16);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setExpandsOnDoubleClick(false);
    m_treeView->setMouseTracking(true);
    m_treeView->setFocusPolicy(Qt::StrongFocus);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setFrameShape(QFrame::NoFrame);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeView->setDefaultDropAction(Qt::MoveAction);

    m_pathEdit = nullptr; // no longer displayed

    m_openBtn = new QPushButton("Open Directory…", this);
    m_openBtn->setFlat(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_openBtn);
    layout->addWidget(m_treeView, 1);

    applyStyle();

    // Async git process
    m_gitProc = new QProcess(this);

    // Git status refresh timer — adaptive poll: 10s → 30s → 60s when idle
    m_gitTimer = new QTimer(this);
    m_gitPollInterval = 10000;
    m_gitTimer->setInterval(m_gitPollInterval);
    connect(m_gitTimer, &QTimer::timeout, this, [this]() {
        if (!m_gitBusy) startGitRefresh();
    });

    // Debounce timer for filesystem watcher — coalesce rapid changes into one refresh
    m_gitDebounce = new QTimer(this);
    m_gitDebounce->setSingleShot(true);
    m_gitDebounce->setInterval(150);
    connect(m_gitDebounce, &QTimer::timeout, this, &FileBrowser::startGitRefresh);

    // SSH git debounce — longer interval to avoid spawning SSH connections too rapidly
    m_sshGitDebounce = new QTimer(this);
    m_sshGitDebounce->setSingleShot(true);
    m_sshGitDebounce->setInterval(1500);
    connect(m_sshGitDebounce, &QTimer::timeout, this, &FileBrowser::startGitRefresh);

    // Filesystem watcher for instant git status updates
    m_fsWatcher = new QFileSystemWatcher(this);
    connect(m_fsWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        m_gitPollInterval = 10000;
        m_gitTimer->setInterval(m_gitPollInterval);
        if (isSshActive())
            m_sshGitDebounce->start();
        else
            m_gitDebounce->start();
        // Re-add file watch — QFileSystemWatcher drops files after atomic replace (rename-over)
        // Use a short delay so the new file is fully written before re-watching
        QTimer::singleShot(50, this, [this, path]() {
            if (!path.isEmpty() && QFile::exists(path) && !m_fsWatcher->files().contains(path))
                m_fsWatcher->addPath(path);
        });
    });
    connect(m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        m_gitPollInterval = 10000;
        m_gitTimer->setInterval(m_gitPollInterval);
        if (isSshActive())
            m_sshGitDebounce->start();
        else
            m_gitDebounce->start();
    });

    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (isSshActive()) {
            // Browse within the SSH mount point
            QString dir = QFileDialog::getExistingDirectory(
                this, "Open Remote Directory", m_currentRoot);
            if (!dir.isEmpty() && dir.startsWith(m_sshMountPoint)) {
                setRootPath(dir);
                emit rootPathOpenedByDialog(dir);
            }
            return;
        }
        QString dir = QFileDialog::getExistingDirectory(this, "Open Directory", m_currentRoot);
        if (!dir.isEmpty()) {
            setRootPath(dir);
            emit rootPathOpenedByDialog(dir);
        }
    });

    connect(m_treeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        if (isDir(index)) {
            if (m_treeView->isExpanded(index))
                m_treeView->collapse(index);
            else
                m_treeView->expand(index);
        } else {
            emit fileOpened(filePath(index));
        }
    });

    connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        if (isSshActive())
            onSshItemExpanded(index);
    });

    connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (isDir(index))
            setRootPath(filePath(index));
    });

    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &FileBrowser::showContextMenu);

    setRootPath(QDir::homePath());
}

// ── Unified accessors ───────────────────────────────────────────────

QString FileBrowser::filePath(const QModelIndex &index) const
{
    if (isSshActive())
        return index.data(FilePathRole).toString();
    QModelIndex srcIdx = m_proxyModel->mapToSource(index);
    return m_fsModel->filePath(srcIdx);
}

bool FileBrowser::isDir(const QModelIndex &index) const
{
    if (isSshActive())
        return index.data(IsDirRole).toBool();
    QModelIndex srcIdx = m_proxyModel->mapToSource(index);
    return m_fsModel->isDir(srcIdx);
}

QString FileBrowser::fileName(const QModelIndex &index) const
{
    return index.data(Qt::DisplayRole).toString();
}

// ── SSH model population ────────────────────────────────────────────

void FileBrowser::sshPopulateDir(QStandardItem *parentItem, const QString &dirPath)
{
    // Remove old children
    parentItem->removeRows(0, parentItem->rowCount());

    QDir dir(dirPath);
    auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const auto &info : entries) {
        QString name = info.fileName();
        QString fullPath = info.absoluteFilePath();

        // Filter .git
        if (name == ".git" && m_dotGitVis == "hidden")
            continue;

        // Filter gitignored
        if (m_gitignoreVis == "hidden" && m_hasGit && gitStatus(fullPath) == Ignored)
            continue;

        auto *item = new QStandardItem(name);
        item->setData(fullPath, FilePathRole);
        item->setData(info.isDir(), IsDirRole);
        item->setEditable(false);

        if (info.isDir()) {
            auto *dummy = new QStandardItem();
            item->appendRow(dummy);
        }

        parentItem->appendRow(item);
    }
}

void FileBrowser::onSshItemExpanded(const QModelIndex &index)
{
    auto *item = m_sshModel->itemFromIndex(index);
    if (!item) return;

    QString path = item->data(FilePathRole).toString();
    if (path.isEmpty()) return;

    // Check if children are just a dummy placeholder
    if (item->rowCount() == 1 && item->child(0)->data(FilePathRole).toString().isEmpty()) {
        sshPopulateDir(item, path);
    }
}

// ── Style ───────────────────────────────────────────────────────────

void FileBrowser::setThemeColors(const QColor &hoverBg, const QColor &selectedBg, const QColor &lineHighlight)
{
    m_hoverBg = hoverBg;
    m_selectedBg = selectedBg;
    m_lineHighlight = lineHighlight;
    m_delegate->setThemeColors(hoverBg, selectedBg, lineHighlight);
    applyStyle();
    m_treeView->viewport()->update();
}

void FileBrowser::highlightFile(const QString &filePath)
{
    m_highlightedFile = filePath;
    if (filePath.isEmpty()) {
        m_treeView->selectionModel()->clearSelection();
        return;
    }
    // For local filesystem model, select the file in the tree
    if (!m_sshMountPoint.isEmpty())
        return;
    QModelIndex srcIdx = m_fsModel->index(filePath);
    if (!srcIdx.isValid())
        return;
    QModelIndex proxyIdx = m_proxyModel->mapFromSource(srcIdx);
    if (!proxyIdx.isValid())
        return;
    m_treeView->setCurrentIndex(proxyIdx);
    m_treeView->scrollTo(proxyIdx);
}

void FileBrowser::applyStyle()
{
    QString bg = m_bgColor.name();
    QString hoverBg = m_hoverBg.isValid() ? m_hoverBg.name()
        : (m_dark ? QColor(m_bgColor.lighter(120)).name()
                  : QColor(m_bgColor.darker(110)).name());
    QString lineHL = m_lineHighlight.isValid() ? m_lineHighlight.name()
        : (m_dark ? QColor(m_bgColor.lighter(108)).name()
                  : QColor(m_bgColor.darker(104)).name());
    QString selBg = m_selectedBg.isValid() ? m_selectedBg.name()
        : (m_dark ? QColor(m_bgColor.lighter(140)).name()
                  : QColor(m_bgColor.darker(120)).name());

    setStyleSheet(QString(R"(
        FileBrowser { background-color: %1; }
        QTreeView {
            background-color: %1; color: %2; border: none; outline: none;
        }
        QTreeView::item { padding: 1px 0px; border: none; }
        QTreeView::item:hover { background-color: %3; }
        QTreeView::item:selected { background-color: %5; }
        QTreeView::branch { background-color: %1; }
        QTreeView::branch:has-children:closed,
        QTreeView::branch:has-children:open { image: none; }
        QPushButton { color: %2; background: %3; border: none; padding: 4px 8px; text-align: left; }
        QPushButton:hover { background-color: %4; }
        QScrollBar:vertical {
            background: %1; width: 8px;
        }
        QScrollBar::handle:vertical {
            background: %4; border-radius: 4px; min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )").arg(bg, m_textColor.name(), hoverBg, selBg, lineHL));
}

// ── Async git status pipeline ───────────────────────────────────────

void FileBrowser::startGitRefresh()
{
    if (m_currentRoot.isEmpty()) return;

    // For SSH mounts, run git commands remotely via ssh (not on sshfs mount)
    if (isSshActive()) {
        startSshGitRefresh();
        return;
    }

    // Cancel any ongoing git process
    if (m_gitBusy) {
        disconnect(m_gitProc, nullptr, this, nullptr);
        if (m_gitProc->state() != QProcess::NotRunning) {
            m_gitProc->kill();
            connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this](int, QProcess::ExitStatus) {
                m_gitBusy = false;
                startGitRefresh();
            });
            return;
        }
    }

    m_gitBusy = true;

    // First call: discover git root, then cache it for subsequent refreshes
    if (m_gitRoot.isEmpty()) {
        m_gitProc->setWorkingDirectory(m_currentRoot);
        disconnect(m_gitProc, nullptr, this, nullptr);
        connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &FileBrowser::onGitDiscoverFinished);
        m_gitProc->start("git", {"rev-parse", "--show-toplevel"});
    } else {
        // Cached git root — go straight to status
        startGitStatusOnly();
    }
}

void FileBrowser::startGitStatusOnly()
{
    m_gitProc->setWorkingDirectory(m_gitRoot);
    disconnect(m_gitProc, nullptr, this, nullptr);
    connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FileBrowser::onLocalGitFinished);
    // Single command: --ignored shows !! entries, --no-optional-locks avoids blocking
    m_gitProc->start("git", {"--no-optional-locks", "status", "--porcelain", "-unormal", "--ignored"});
}

void FileBrowser::onGitDiscoverFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);

    if (exitCode != 0) {
        m_hasGit = false;
        m_gitBusy = false;
        m_gitTimer->stop();
        bool changed = !m_modified.isEmpty() || !m_untracked.isEmpty() || !m_added.isEmpty() || !m_ignored.isEmpty();
        m_modified.clear(); m_untracked.clear(); m_added.clear(); m_ignored.clear();
        m_dirModified.clear(); m_dirUntracked.clear(); m_dirAdded.clear(); m_dirIgnored.clear();
        if (changed) m_treeView->viewport()->update();
        return;
    }

    m_hasGit = true;
    m_gitRoot = QString::fromUtf8(m_gitProc->readAllStandardOutput()).trimmed();

    // Watch .git/ directory — catches index, HEAD, refs changes all at once
    QString gitDir = m_gitRoot + "/.git";
    if (QFileInfo(gitDir).isDir() && !m_fsWatcher->directories().contains(gitDir)
        && m_fsWatcher->directories().size() < 4000)
        m_fsWatcher->addPath(gitDir);
    // Watch .gitignore for ignore rule changes
    QString gitignorePath = m_gitRoot + "/.gitignore";
    if (QFile::exists(gitignorePath) && !m_fsWatcher->files().contains(gitignorePath)
        && m_fsWatcher->files().size() < 4000)
        m_fsWatcher->addPath(gitignorePath);

    if (!m_gitTimer->isActive())
        m_gitTimer->start();

    // Now run the actual status
    startGitStatusOnly();
}

void FileBrowser::startSshGitRefresh()
{
    if (!m_sshManager || m_sshManager->activeProfileIndex() < 0) {
        m_gitBusy = false;
        return;
    }

    if (m_gitBusy) {
        disconnect(m_gitProc, nullptr, this, nullptr);
        if (m_gitProc->state() != QProcess::NotRunning) {
            m_gitProc->kill();
            connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this](int, QProcess::ExitStatus) {
                m_gitBusy = false;
                startSshGitRefresh();
            });
            return;
        }
    }

    m_gitBusy = true;

    const auto &cfg = m_sshManager->profileConfig(m_sshManager->activeProfileIndex());
    QString remotePath = toRemotePath(m_currentRoot);

    // Single SSH command: git root + status + ls-files ignored, separated by markers
    QString cmd = QString(
        "cd %1 2>/dev/null && "
        "git rev-parse --is-inside-work-tree 2>/dev/null && "
        "echo '%%GIT_ROOT%%' && "
        "git rev-parse --show-toplevel && "
        "echo '%%GIT_STATUS%%' && "
        "git status --porcelain -unormal && "
        "echo '%%GIT_IGNORED%%' && "
        "git ls-files --others --ignored --exclude-standard --directory"
    ).arg(QString("'%1'").arg(remotePath.replace("'", "'\\''")));

    disconnect(m_gitProc, nullptr, this, nullptr);
    connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FileBrowser::onSshGitFinished);

    QStringList args;
    args << "-o" << "StrictHostKeyChecking=accept-new"
         << "-o" << "ConnectTimeout=5"
         << "-o" << "ControlMaster=auto"
         << "-o" << QString("ControlPath=/tmp/vibe-ssh-%1-%r@%h:%p").arg(QCoreApplication::applicationPid())
         << "-o" << "ControlPersist=120"
         << "-p" << QString::number(cfg.port);
    if (!cfg.identityFile.isEmpty() && QFileInfo(cfg.identityFile).exists())
        args << "-i" << cfg.identityFile;
    args << (cfg.user + "@" + cfg.host) << cmd;

    if (!cfg.password.isEmpty())
        m_sshManager->setupSshpassEnv(m_gitProc, cfg.password);

    if (!cfg.password.isEmpty())
        m_gitProc->start("sshpass", QStringList() << "-e" << "ssh" << args);
    else
        m_gitProc->start("ssh", args);
}

void FileBrowser::onSshGitFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);
    m_gitBusy = false;

    QByteArray raw = m_gitProc->readAllStandardOutput();
    QString output = QString::fromUtf8(raw);

    // First line should be "true" from rev-parse --is-inside-work-tree
    if (!output.startsWith("true")) {
        m_hasGit = false;
        bool changed = !m_modified.isEmpty() || !m_untracked.isEmpty() || !m_added.isEmpty() || !m_ignored.isEmpty();
        m_modified.clear(); m_untracked.clear(); m_added.clear(); m_ignored.clear();
        m_dirModified.clear(); m_dirUntracked.clear(); m_dirAdded.clear(); m_dirIgnored.clear();
        if (changed) m_treeView->viewport()->update();
        return;
    }

    m_hasGit = true;
    if (!m_gitTimer->isActive())
        m_gitTimer->start();

    // Parse sections separated by markers
    int rootIdx = output.indexOf("%%GIT_ROOT%%");
    int statusIdx = output.indexOf("%%GIT_STATUS%%");
    int ignoredIdx = output.indexOf("%%GIT_IGNORED%%");

    if (rootIdx < 0 || statusIdx < 0) return;

    QString remoteGitRoot = output.mid(rootIdx + 12, statusIdx - rootIdx - 12).trimmed();
    m_gitRoot = m_sshMountPoint + remoteGitRoot;

    int statusEnd = (ignoredIdx >= 0) ? ignoredIdx : output.length();
    QString statusOutput = output.mid(statusIdx + 14, statusEnd - statusIdx - 14).trimmed();
    QString ignoredOutput = (ignoredIdx >= 0) ? output.mid(ignoredIdx + 15).trimmed() : QString();

    // Parse status + ignored into sets
    QSet<QString> newModified, newUntracked, newAdded, newIgnored;

    for (const QString &line : statusOutput.split('\n', Qt::SkipEmptyParts)) {
        if (line.length() < 4) continue;
        QChar x = line[0], y = line[1];
        QString file = line.mid(3).trimmed();
        if (file.contains(" -> ")) file = file.split(" -> ").last();
        if (file.endsWith('/')) file.chop(1);
        QString fullPath = m_gitRoot + "/" + file;

        if (x == '?' && y == '?') newUntracked.insert(fullPath);
        else if (x == 'A') newAdded.insert(fullPath);
        else if (y == 'M' || x == 'M') newModified.insert(fullPath);
        else if (x == 'D' || y == 'D') newModified.insert(fullPath);
    }

    for (const QString &line : ignoredOutput.split('\n', Qt::SkipEmptyParts)) {
        QString file = line.trimmed();
        if (file.endsWith('/')) file.chop(1);
        if (!file.isEmpty()) newIgnored.insert(m_gitRoot + "/" + file);
    }

    bool changed = (newModified != m_modified || newUntracked != m_untracked
                     || newAdded != m_added || newIgnored != m_ignored);
    m_modified = newModified;
    m_untracked = newUntracked;
    m_added = newAdded;
    m_ignored = newIgnored;

    if (changed) {
        rebuildDirCache();
        m_treeView->viewport()->update();
        m_gitPollInterval = 10000;
    } else {
        m_gitPollInterval = qMin(m_gitPollInterval * 2, 60000);
    }
    if (m_gitTimer->isActive())
        m_gitTimer->setInterval(m_gitPollInterval);
}

void FileBrowser::onLocalGitFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);
    m_gitBusy = false;

    if (exitCode != 0 && exitCode != 128) {
        // git root may have changed (e.g. moved outside repo) — rediscover
        m_gitRoot.clear();
        return;
    }

    QByteArray raw = m_gitProc->readAllStandardOutput();
    if (raw.size() > 2 * 1024 * 1024) raw.truncate(2 * 1024 * 1024);

    // Parse git status --porcelain --ignored output directly
    // Format: XY filename  (!! = ignored, ?? = untracked, M = modified, A = added, etc.)
    QSet<QString> newModified, newUntracked, newAdded, newIgnored;

    const QStringList lines = QString::fromUtf8(raw).split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.length() < 4) continue;
        QChar x = line[0];
        QChar y = line[1];
        QString file = line.mid(3).trimmed();
        if (file.contains(" -> "))
            file = file.split(" -> ").last();
        if (file.endsWith('/'))
            file.chop(1);

        QString fullPath = m_gitRoot + "/" + file;

        if (x == '!' && y == '!') {
            newIgnored.insert(fullPath);
        } else if (x == '?' && y == '?') {
            newUntracked.insert(fullPath);
        } else if (x == 'A') {
            newAdded.insert(fullPath);
        } else if (y == 'M' || x == 'M') {
            newModified.insert(fullPath);
        } else if (x == 'D' || y == 'D') {
            newModified.insert(fullPath);
        }
    }

    bool changed = (newModified != m_modified || newUntracked != m_untracked
                     || newAdded != m_added || newIgnored != m_ignored);
    m_modified = newModified;
    m_untracked = newUntracked;
    m_added = newAdded;
    m_ignored = newIgnored;

    // Re-ensure watches (atomic saves drop file watches)
    if (!m_gitRoot.isEmpty()) {
        QString gitignorePath = m_gitRoot + "/.gitignore";
        if (QFile::exists(gitignorePath) && !m_fsWatcher->files().contains(gitignorePath))
            m_fsWatcher->addPath(gitignorePath);
        QString gitDir = m_gitRoot + "/.git";
        if (QFileInfo(gitDir).isDir() && !m_fsWatcher->directories().contains(gitDir))
            m_fsWatcher->addPath(gitDir);
    }

    if (changed) {
        rebuildDirCache();
        m_proxyModel->refresh();
        m_treeView->viewport()->update();
        m_gitPollInterval = 10000;
    } else {
        m_gitPollInterval = qMin(m_gitPollInterval * 2, 60000);
    }
    if (m_gitTimer->isActive())
        m_gitTimer->setInterval(m_gitPollInterval);
}

// Legacy handlers removed — local git now uses onGitDiscoverFinished + onLocalGitFinished

void FileBrowser::rebuildDirCache()
{
    m_dirModified.clear();
    m_dirUntracked.clear();
    m_dirAdded.clear();
    m_dirIgnored.clear();

    auto addParents = [](const QSet<QString> &files, QSet<QString> &dirs) {
        for (const auto &f : files) {
            QString dir = QFileInfo(f).absolutePath();
            while (!dir.isEmpty() && dir != "/") {
                if (dirs.contains(dir)) break;
                dirs.insert(dir);
                dir = QFileInfo(dir).absolutePath();
            }
        }
    };

    addParents(m_modified, m_dirModified);
    addParents(m_untracked, m_dirUntracked);
    addParents(m_added, m_dirAdded);
    // Note: do NOT propagate ignored status to parent directories —
    // a directory containing an ignored file is not itself ignored.
}

FileBrowser::GitStatus FileBrowser::gitStatus(const QString &fp) const
{
    if (m_ignored.contains(fp)) return Ignored;
    if (m_modified.contains(fp)) return Modified;
    if (m_untracked.contains(fp)) return Untracked;
    if (m_added.contains(fp)) return Added;

    // Check if fp is inside a reported directory (child lookup)
    // git status -unormal reports directories instead of individual files
    for (const QString &path : m_ignored) {
        if (fp.startsWith(path + '/')) return Ignored;
    }
    for (const QString &path : m_untracked) {
        if (fp.startsWith(path + '/')) return Untracked;
    }
    for (const QString &path : m_added) {
        if (fp.startsWith(path + '/')) return Added;
    }
    for (const QString &path : m_modified) {
        if (fp.startsWith(path + '/')) return Modified;
    }

    // Parent directory propagation (directory contains modified/untracked/added children)
    if (m_dirModified.contains(fp)) return Modified;
    if (m_dirUntracked.contains(fp)) return Untracked;
    if (m_dirAdded.contains(fp)) return Added;

    return Clean;
}

// ── Context menu ────────────────────────────────────────────────────

QString FileBrowser::selectedDirPath() const
{
    QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid())
        return m_currentRoot;
    QString path = filePath(idx);
    if (isDir(idx))
        return path;
    return QFileInfo(path).absolutePath();
}

void FileBrowser::showContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_treeView->indexAt(pos);
    QString dirPath = selectedDirPath();

    QMenu menu(this);
    // Menu colors handled by global theme stylesheet

    menu.addAction("New File...", this, [this, dirPath]() {
        bool ok;
        QString name = QInputDialog::getText(this, "New File", "File name:",
                                              QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            if (name.contains('/') || name.contains("..") || name.contains('\0') || name.contains('\n')) {
                ThemedMessageBox::warning(this, "Error", "Invalid file name.");
                return;
            }
            QFile file(dirPath + "/" + name);
            if (file.open(QIODevice::WriteOnly)) file.close();
            if (isSshActive()) setRootPath(m_currentRoot); // refresh
        }
    });

    menu.addAction("New Directory...", this, [this, dirPath]() {
        bool ok;
        QString name = QInputDialog::getText(this, "New Directory", "Directory name:",
                                              QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            if (name.contains('/') || name.contains("..") || name.contains('\0') || name.contains('\n')) {
                ThemedMessageBox::warning(this, "Error", "Invalid directory name.");
                return;
            }
            if (!QDir(dirPath).mkdir(name))
                ThemedMessageBox::warning(this, "Error", "Failed to create directory.");
            if (isSshActive()) setRootPath(m_currentRoot); // refresh
        }
    });

    menu.addSeparator();

    if (idx.isValid()) {
        QString itemPath = filePath(idx);
        QString itemName = fileName(idx);

        menu.addAction("Rename...", this, [this, idx, itemPath, itemName]() {
            bool ok;
            QString newName = QInputDialog::getText(this, "Rename", "New name:",
                                                     QLineEdit::Normal, itemName, &ok);
            if (ok && !newName.isEmpty() && newName != itemName) {
                if (newName.contains('/') || newName.contains("..") || newName.contains('\0') || newName.contains('\n')) {
                    ThemedMessageBox::warning(this, "Error", "Invalid name.");
                    return;
                }
                QString dir = QFileInfo(itemPath).absolutePath();
                QFile::rename(itemPath, dir + "/" + newName);
                if (isSshActive()) setRootPath(m_currentRoot);
            }
        });

        menu.addAction("Delete", this, [this, idx, itemPath, itemName]() {
            auto reply = ThemedMessageBox::warning(this, "Delete",
                QString("Delete '%1'?\n\nFull path: %2").arg(itemName, itemPath),
                ThemedMessageBox::Yes | ThemedMessageBox::No, ThemedMessageBox::No);
            if (reply == ThemedMessageBox::Yes) {
                if (isDir(idx)) {
                    if (!QDir(itemPath).removeRecursively())
                        ThemedMessageBox::warning(this, "Error", "Failed to delete directory.");
                } else {
                    if (!QFile::remove(itemPath))
                        ThemedMessageBox::warning(this, "Error", "Failed to delete file.");
                }
                if (isSshActive()) setRootPath(m_currentRoot);
            }
        });
    }

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

// ── Root path ───────────────────────────────────────────────────────

void FileBrowser::setRootPath(const QString &path)
{
    m_currentRoot = path;

    if (isSshActive()) {
        // Use QStandardItemModel with synchronous QDir reads
        m_treeView->setModel(m_sshModel);
        m_sshModel->clear();

        auto *root = m_sshModel->invisibleRootItem();
        sshPopulateDir(root, path);

        m_treeView->setRootIndex(QModelIndex());
    } else {
        // Use normal QFileSystemModel through proxy
        if (m_treeView->model() != m_proxyModel) {
            m_treeView->setModel(m_proxyModel);
            m_treeView->hideColumn(1);
            m_treeView->hideColumn(2);
            m_treeView->hideColumn(3);
        }
        QModelIndex rootIndex = m_fsModel->setRootPath(path);
        m_treeView->setRootIndex(m_proxyModel->mapFromSource(rootIndex));
    }

    // Show directory name on the button
    QString dirName = QFileInfo(path).fileName();
    if (dirName.isEmpty()) dirName = path;
    if (isSshActive()) {
        QString remotePath = toRemotePath(path);
        QString remoteDirName = remotePath.section('/', -1);
        if (remoteDirName.isEmpty()) remoteDirName = remotePath;
        m_openBtn->setText(remoteDirName);
    } else {
        m_openBtn->setText(dirName);
    }

    // Rewire filesystem watcher for new root (skip on SSH — avoids blocking FUSE calls)
    if (!m_fsWatcher->directories().isEmpty())
        m_fsWatcher->removePaths(m_fsWatcher->directories());
    if (!m_fsWatcher->files().isEmpty())
        m_fsWatcher->removePaths(m_fsWatcher->files());
    if (!isSshActive() && m_fsWatcher->directories().size() < 4000)
        m_fsWatcher->addPath(path);

    m_gitRoot.clear(); // force rediscovery of git root for new path
    startGitRefresh();
    emit rootPathChanged(path);
}

void FileBrowser::setSshMount(const QString &mountPoint, const QString &remotePrefix)
{
    m_sshMountPoint = mountPoint;
    m_sshRemotePrefix = remotePrefix;
}

void FileBrowser::clearSshMount()
{
    m_sshMountPoint.clear();
    m_sshRemotePrefix.clear();
}

QString FileBrowser::toRemotePath(const QString &localPath) const
{
    if (m_sshMountPoint.isEmpty())
        return localPath;
    if (localPath.startsWith(m_sshMountPoint)) {
        QString relative = localPath.mid(m_sshMountPoint.length());
        if (relative.isEmpty())
            return "/";
        return relative; // already starts with /
    }
    return localPath;
}

void FileBrowser::setFont(const QFont &font)
{
    m_treeView->setFont(font);
    m_openBtn->setFont(font);
}

void FileBrowser::setTheme(const QString &theme, const QColor &bg, const QColor &fg)
{
    m_dark = (theme == "Dark");
    if (bg.isValid() && fg.isValid()) {
        m_bgColor = bg;
        m_textColor = fg;
    } else if (m_dark) {
        m_bgColor = QColor("#1e1e1e");
        m_textColor = QColor("#d4d4d4");
    } else {
        m_bgColor = QColor("#f5f5f5");
        m_textColor = QColor("#24292f");
    }
    applyStyle();
    m_treeView->viewport()->update();
}

void FileBrowser::setGitignoreVisibility(const QString &mode)
{
    m_gitignoreVis = mode;
    m_proxyModel->refresh();
    m_treeView->viewport()->update();
}

void FileBrowser::setDotGitVisibility(const QString &mode)
{
    m_dotGitVis = mode;
    m_proxyModel->refresh();
    m_treeView->viewport()->update();
}

QString FileBrowser::rootPath() const
{
    return m_currentRoot;
}
