#include "filebrowser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QFileInfo>

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
        painter->fillRect(option.rect, dark ? QColor("#37373d") : QColor("#d0d0d0"));
    else if (option.state & QStyle::State_MouseOver)
        painter->fillRect(option.rect, dark ? QColor("#2a2d2e") : QColor("#e8e8e8"));

    QRect textRect = option.rect;
    textRect.setLeft(textRect.left() + 4);

    // Determine color
    QColor textColor;
    QString icon;

    // Check git status first
    bool gitColored = false;
    if (m_fb && m_fb->hasGit() && !fullPath.isEmpty()) {
        auto status = m_fb->gitStatus(fullPath);
        if (status == FileBrowser::Modified) {
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
            else if (name.startsWith("."))
                textColor = dark ? QColor("#858585") : QColor("#8b949e");
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

// ── FileBrowser ─────────────────────────────────────────────────────

FileBrowser::FileBrowser(QWidget *parent)
    : QWidget(parent)
{
    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setReadOnly(false);
    m_fsModel->setFilter(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);

    m_sshModel = new QStandardItemModel(this);

    m_delegate = new FileItemDelegate(this);
    m_delegate->setFileBrowser(this);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_fsModel);
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

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("Directory path...");
    m_pathEdit->setFrame(false);

    m_openBtn = new QPushButton("\u2026", this);
    m_openBtn->setFixedWidth(30);
    m_openBtn->setFlat(true);

    auto *topLayout = new QHBoxLayout;
    topLayout->setContentsMargins(4, 2, 4, 2);
    topLayout->setSpacing(2);
    topLayout->addWidget(m_pathEdit, 1);
    topLayout->addWidget(m_openBtn);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(topLayout);
    layout->addWidget(m_treeView, 1);

    applyStyle();

    // Async git process
    m_gitProc = new QProcess(this);

    // Git status refresh timer — 5s interval to reduce CPU usage
    m_gitTimer = new QTimer(this);
    m_gitTimer->setInterval(5000);
    connect(m_gitTimer, &QTimer::timeout, this, &FileBrowser::startGitRefresh);
    m_gitTimer->start();

    connect(m_openBtn, &QPushButton::clicked, this, [this]() {
        if (isSshActive()) return; // no local dialog for SSH
        QString dir = QFileDialog::getExistingDirectory(this, "Open Directory", m_pathEdit->text());
        if (!dir.isEmpty())
            setRootPath(dir);
    });

    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
        QString text = m_pathEdit->text();
        if (isSshActive()) {
            // User types remote path like /opt — translate to local mount
            if (text.startsWith("/"))
                text = m_sshMountPoint + text;
        }
        setRootPath(text);
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
    return m_fsModel->filePath(index);
}

bool FileBrowser::isDir(const QModelIndex &index) const
{
    if (isSshActive())
        return index.data(IsDirRole).toBool();
    return m_fsModel->isDir(index);
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
        auto *item = new QStandardItem(info.fileName());
        item->setData(info.absoluteFilePath(), FilePathRole);
        item->setData(info.isDir(), IsDirRole);
        item->setEditable(false);

        if (info.isDir()) {
            // Add a dummy child so the expand arrow shows
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

void FileBrowser::applyStyle()
{
    QString bg = m_bgColor.name();
    QString hoverBg = QColor(m_bgColor.lighter(120)).name();
    QString selBg = QColor(m_bgColor.lighter(140)).name();

    setStyleSheet(QString(R"(
        FileBrowser { background-color: %1; }
        QTreeView {
            background-color: %1; color: %2; border: none; outline: none;
        }
        QTreeView::item { padding: 1px 0px; border: none; }
        QTreeView::item:hover { background-color: %3; }
        QTreeView::item:selected { background-color: %4; }
        QTreeView::branch { background-color: %1; }
        QTreeView::branch:has-children:closed,
        QTreeView::branch:has-children:open { image: none; }
        QLineEdit {
            background-color: %3; color: %2;
            border: 1px solid %4; padding: 3px 6px; border-radius: 3px;
        }
        QLineEdit:focus { border-color: #007acc; }
        QPushButton { color: %2; background: transparent; font-size: 14px; }
        QPushButton:hover { background-color: %4; border-radius: 3px; }
        QScrollBar:vertical {
            background: %1; width: 8px;
        }
        QScrollBar::handle:vertical {
            background: %4; border-radius: 4px; min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )").arg(bg, m_textColor.name(), hoverBg, selBg));
}

// ── Async git status pipeline ───────────────────────────────────────

void FileBrowser::startGitRefresh()
{
    if (m_gitBusy) return;
    if (m_currentRoot.isEmpty()) return;

    m_gitBusy = true;
    m_gitProc->setWorkingDirectory(m_currentRoot);

    disconnect(m_gitProc, nullptr, this, nullptr);
    connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FileBrowser::onGitCheckFinished);
    m_gitProc->start("git", {"rev-parse", "--is-inside-work-tree"});
}

void FileBrowser::onGitCheckFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);

    if (exitCode != 0) {
        m_hasGit = false;
        bool changed = !m_modified.isEmpty() || !m_untracked.isEmpty() || !m_added.isEmpty();
        m_modified.clear();
        m_untracked.clear();
        m_added.clear();
        m_dirModified.clear();
        m_dirUntracked.clear();
        m_dirAdded.clear();
        m_gitBusy = false;
        if (changed)
            m_treeView->viewport()->update();
        return;
    }

    m_hasGit = true;

    connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FileBrowser::onGitRootFinished);
    m_gitProc->start("git", {"rev-parse", "--show-toplevel"});
}

void FileBrowser::onGitRootFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);

    if (exitCode != 0) {
        m_gitBusy = false;
        return;
    }

    m_gitRoot = QString::fromUtf8(m_gitProc->readAllStandardOutput()).trimmed();

    connect(m_gitProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FileBrowser::onGitStatusFinished);
    m_gitProc->start("git", {"status", "--porcelain", "-unormal"});
}

void FileBrowser::onGitStatusFinished(int exitCode, QProcess::ExitStatus)
{
    disconnect(m_gitProc, nullptr, this, nullptr);
    m_gitBusy = false;

    if (exitCode != 0) return;

    m_gitStatusOutput = QString::fromUtf8(m_gitProc->readAllStandardOutput());
    parseGitOutput();
}

void FileBrowser::parseGitOutput()
{
    QSet<QString> newModified, newUntracked, newAdded;

    for (const QString &line : m_gitStatusOutput.split('\n', Qt::SkipEmptyParts)) {
        if (line.length() < 4) continue;
        QChar x = line[0];
        QChar y = line[1];
        QString file = line.mid(3).trimmed();
        if (file.contains(" -> "))
            file = file.split(" -> ").last();

        QString fullPath = m_gitRoot + "/" + file;

        if (x == '?' && y == '?') {
            newUntracked.insert(fullPath);
        } else if (x == 'A') {
            newAdded.insert(fullPath);
        } else if (y == 'M' || x == 'M') {
            newModified.insert(fullPath);
        } else if (x == 'D' || y == 'D') {
            newModified.insert(fullPath);
        }
    }

    bool changed = (newModified != m_modified || newUntracked != m_untracked || newAdded != m_added);
    m_modified = newModified;
    m_untracked = newUntracked;
    m_added = newAdded;

    if (changed) {
        rebuildDirCache();
        m_treeView->viewport()->update();
    }
}

void FileBrowser::rebuildDirCache()
{
    m_dirModified.clear();
    m_dirUntracked.clear();
    m_dirAdded.clear();

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
}

FileBrowser::GitStatus FileBrowser::gitStatus(const QString &fp) const
{
    if (m_modified.contains(fp)) return Modified;
    if (m_untracked.contains(fp)) return Untracked;
    if (m_added.contains(fp)) return Added;

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
    if (m_dark) {
        menu.setStyleSheet(R"(
            QMenu { background-color: #2d2d2d; color: #cccccc; border: 1px solid #454545; padding: 4px 0px; }
            QMenu::item { padding: 5px 20px; }
            QMenu::item:selected { background-color: #094771; }
            QMenu::separator { height: 1px; background: #454545; margin: 4px 8px; }
        )");
    } else {
        menu.setStyleSheet(R"(
            QMenu { background-color: #f5f5f5; color: #24292f; border: 1px solid #d0d0d0; padding: 4px 0px; }
            QMenu::item { padding: 5px 20px; }
            QMenu::item:selected { background-color: #0969da; color: #ffffff; }
            QMenu::separator { height: 1px; background: #d0d0d0; margin: 4px 8px; }
        )");
    }

    menu.addAction("New File...", this, [this, dirPath]() {
        bool ok;
        QString name = QInputDialog::getText(this, "New File", "File name:",
                                              QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            if (name.contains('/') || name.contains("..")) {
                QMessageBox::warning(this, "Error", "Invalid file name.");
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
            if (name.contains('/') || name.contains("..")) {
                QMessageBox::warning(this, "Error", "Invalid directory name.");
                return;
            }
            QDir(dirPath).mkdir(name);
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
                if (newName.contains('/') || newName.contains("..")) {
                    QMessageBox::warning(this, "Error", "Invalid name.");
                    return;
                }
                QString dir = QFileInfo(itemPath).absolutePath();
                QFile::rename(itemPath, dir + "/" + newName);
                if (isSshActive()) setRootPath(m_currentRoot);
            }
        });

        menu.addAction("Delete", this, [this, idx, itemPath, itemName]() {
            auto reply = QMessageBox::warning(this, "Delete",
                QString("Delete '%1'?\n\nFull path: %2").arg(itemName, itemPath),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                if (isDir(idx)) QDir(itemPath).removeRecursively();
                else QFile::remove(itemPath);
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
        // Use normal QFileSystemModel
        if (m_treeView->model() != m_fsModel) {
            m_treeView->setModel(m_fsModel);
            m_treeView->hideColumn(1);
            m_treeView->hideColumn(2);
            m_treeView->hideColumn(3);
        }
        QModelIndex rootIndex = m_fsModel->setRootPath(path);
        m_treeView->setRootIndex(rootIndex);
    }

    m_pathEdit->setText(isSshActive() ? toRemotePath(path) : path);
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
    m_pathEdit->setFont(font);
}

void FileBrowser::setTheme(const QString &theme)
{
    m_dark = (theme == "Dark");
    if (m_dark) {
        m_bgColor = QColor("#1e1e1e");
        m_textColor = QColor("#d4d4d4");
    } else {
        m_bgColor = QColor("#f5f5f5");
        m_textColor = QColor("#24292f");
    }
    applyStyle();
    m_treeView->viewport()->update();
}

QString FileBrowser::rootPath() const
{
    return m_currentRoot;
}
