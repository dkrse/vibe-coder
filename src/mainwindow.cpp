#include "mainwindow.h"
#include "titlebar.h"
#include "sshtunneldialog.h"
#include <memory>
#include <QCryptographicHash>

#include <QStyleFactory>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include "themeddialog.h"
#include <QFont>
#include <QTextBlockFormat>
#include <QProcess>
#include <QToolBar>
#include <QMenuBar>
#include <QDateTime>
#include <QShortcut>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QStyleHints>
#include <QHoverEvent>
#include <QWindow>
#include <QTimer>
#include <QCloseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWebEngineView>

static QString langFromSuffix(const QString &suffix)
{
    QString s = suffix.toLower();
    if (s == "cpp" || s == "cxx" || s == "cc") return "cpp";
    if (s == "c") return "c";
    if (s == "h" || s == "hpp" || s == "hxx") return "h";
    if (s == "py" || s == "pyw") return "py";
    if (s == "js") return "js";
    if (s == "ts") return "ts";
    if (s == "jsx") return "jsx";
    if (s == "tsx") return "tsx";
    if (s == "rs") return "rs";
    if (s == "md" || s == "markdown" || s == "mkd" || s == "mdx") return "md";
    return "";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Vibe Coder");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_Hover, true);
    resize(1200, 800);

    m_settings.load();
    m_sshManager = new SshManager(this);

    menuBar()->hide();

    auto *central = new QWidget(this);
    setCentralWidget(central);

    // Custom title bar
    m_titleBar = new TitleBar(this);
    connect(m_titleBar, &TitleBar::minimizeClicked, this, &QMainWindow::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeClicked, this, [this]() {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeClicked, this, &QMainWindow::close);

    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_rightSplitter = new QSplitter(Qt::Vertical);

    // Tab widget with hamburger button in corner
    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index == 0) return;
        QWidget *w = m_tabWidget->widget(index);
        // Handle markdown preview tab — delete it
        if (auto *mdp = qobject_cast<MarkdownPreview *>(w)) {
            m_tabWidget->removeTab(index);
            m_mdPreviews.removeOne(mdp);
            mdp->deleteLater();
            return;
        }
        if (!maybeSaveTab(m_tabWidget, index)) return;
        QString path = m_tabWidget->tabToolTip(index);
        if (!path.isEmpty())
            m_fileWatcher->removePath(path);
        m_tabWidget->removeTab(index);
        w->deleteLater();
    });

    // Tab context menu
    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        int clickedIndex = m_tabWidget->tabBar()->tabAt(pos);
        if (clickedIndex < 0) return;

        QMenu menu(this);
        if (clickedIndex > 0) {
            menu.addAction("Close", this, [this, clickedIndex]() {
                closeTab(m_tabWidget, clickedIndex);
            });
        }
        menu.addAction("Close Others", this, [this, clickedIndex]() {
            closeOtherTabs(m_tabWidget, clickedIndex);
        });
        menu.addAction("Close All", this, [this]() {
            closeAllTabs(m_tabWidget);
        });
        if (clickedIndex < m_tabWidget->count() - 1) {
            menu.addAction("Close to the Right", this, [this, clickedIndex]() {
                closeTabsToTheRight(m_tabWidget, clickedIndex);
            });
        }
        if (clickedIndex > 1) {
            menu.addAction("Close to the Left", this, [this, clickedIndex]() {
                closeTabsToTheLeft(m_tabWidget, clickedIndex);
            });
        }
        menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));
    });

    // Hamburger menu
    m_menuBtn = new QToolButton;
    m_menuBtn->setText("\u2630");
    m_menuBtn->setAutoRaise(true);
    m_menuBtn->setPopupMode(QToolButton::InstantPopup);
    m_menuBtn->setStyleSheet("QToolButton { font-size: 16px; padding: 4px 8px; }"
                              "QToolButton::menu-indicator { image: none; }");

    auto *hamburgerMenu = new QMenu(m_menuBtn);
    hamburgerMenu->addAction("Create Project...", this, &MainWindow::onCreateProject);
    hamburgerMenu->addAction("Edit Project...", this, &MainWindow::onEditProject);
    hamburgerMenu->addSeparator();
    m_sshConnectAction = hamburgerMenu->addAction("SSH Connect...", this, &MainWindow::onSshConnect);
    m_sshDisconnectAction = hamburgerMenu->addAction("SSH Disconnect", this, &MainWindow::onSshDisconnect);
    m_sshDisconnectAction->setEnabled(false);
    m_sshTunnelsAction = hamburgerMenu->addAction("SSH Tunnels...", this, &MainWindow::onSshTunnels);
    m_sshTunnelsAction->setEnabled(false);
    m_sshUploadAction = hamburgerMenu->addAction("SSH Upload File...", this, &MainWindow::onSshUpload);
    m_sshUploadAction->setEnabled(false);
    m_sshDownloadAction = hamburgerMenu->addAction("SSH Download File...", this, &MainWindow::onSshDownload);
    m_sshDownloadAction->setEnabled(false);
    hamburgerMenu->addSeparator();
    hamburgerMenu->addAction("Split Horizontal", this, &MainWindow::splitEditorHorizontal);
    hamburgerMenu->addAction("Split Vertical", this, &MainWindow::splitEditorVertical);
    hamburgerMenu->addAction("Unsplit", this, &MainWindow::unsplitEditor);
    hamburgerMenu->addSeparator();
    hamburgerMenu->addAction("Settings...", this, &MainWindow::onSettingsTriggered);
    m_menuBtn->setMenu(hamburgerMenu);

    m_tabWidget->setCornerWidget(m_menuBtn, Qt::TopRightCorner);

    m_terminal = new TerminalWidget;
    m_tabWidget->addTab(m_terminal, "AI-terminal");
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);

    // Bottom tab widget with Prompt and Terminal tabs
    m_bottomTabWidget = new QTabWidget;

    // Prompt tab
    auto *promptTab = new QWidget;
    auto *promptLayout = new QVBoxLayout(promptTab);
    promptLayout->setContentsMargins(0, 0, 0, 0);

    // Saved prompts selector
    auto *savedLayout = new QHBoxLayout;
    m_savedPromptsCombo = new QComboBox;
    m_savedPromptsCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_savedPromptsCombo->addItem("-- Saved prompts --");
    savedLayout->addWidget(m_savedPromptsCombo, 1);

    auto *deletePromptBtn = new QPushButton("X");
    deletePromptBtn->setFixedWidth(28);
    deletePromptBtn->setToolTip("Delete selected saved prompt");
    savedLayout->addWidget(deletePromptBtn);
    promptLayout->addLayout(savedLayout);

    m_editor = new PromptEdit;
    m_editor->setPlaceholderText("Type prompt here...");

    auto *btnLayout = new QHBoxLayout;
    m_sendBtn = new QPushButton("Send");
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setToolTip("Stop model generation");
    m_savePromptBtn = new QPushButton("Save Prompt");
    m_savePromptBtn->setToolTip("Save current prompt for reuse");
    m_repeatPromptBtn = new QPushButton("Repeat");
    m_repeatPromptBtn->setToolTip("Repeat last sent prompt");
    m_repeatPromptBtn->setEnabled(false);
    auto *showTerminalBtn = new QPushButton("AI-terminal");
    showTerminalBtn->setToolTip("Show AI-terminal");
    connect(showTerminalBtn, &QPushButton::clicked, this, [this]() {
        m_tabWidget->setCurrentIndex(0);
    });
    // AI-terminal activity indicator
    m_aiActivityLabel = new QLabel("●");
    m_aiActivityLabel->setFixedWidth(20);
    m_aiActivityLabel->setAlignment(Qt::AlignCenter);
    m_aiActivityLabel->setToolTip("AI-terminal activity");

    m_aiIdleTimer = new QTimer(this);
    m_aiIdleTimer->setSingleShot(true);
    m_aiIdleTimer->setInterval(1500);
    connect(m_aiIdleTimer, &QTimer::timeout, this, [this]() {
        m_aiAnimTimer->stop();
        m_aiActivityLabel->setText("●");
    });

    m_aiAnimTimer = new QTimer(this);
    m_aiAnimTimer->setInterval(200);
    m_aiAnimFrame = 0;
    connect(m_aiAnimTimer, &QTimer::timeout, this, [this]() {
        const QString frames[] = {"|", "/", "-", "\\"};
        m_aiActivityLabel->setText(frames[m_aiAnimFrame % 4]);
        m_aiAnimFrame++;
    });

    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_repeatPromptBtn);
    btnLayout->addWidget(m_savePromptBtn);
    btnLayout->addWidget(showTerminalBtn);
    btnLayout->addWidget(m_aiActivityLabel);
    btnLayout->addStretch();

    // Monitor AI-terminal content changes by comparing screen snapshots
    // Uses raw image bits (no PNG encoding) for minimal CPU overhead
    auto *pollTimer = new QTimer(this);
    pollTimer->setInterval(1000);
    QByteArray lastHash;
    bool initialized = false;
    connect(pollTimer, &QTimer::timeout, this, [this, lastHash, initialized]() mutable {
        QImage img = m_terminal->terminal()->grab().toImage();
        QByteArray raw(reinterpret_cast<const char*>(img.constBits()), img.sizeInBytes());
        QByteArray hash = QCryptographicHash::hash(raw, QCryptographicHash::Md5);
        if (!initialized) {
            lastHash = hash;
            initialized = true;
            return;
        }
        if (hash != lastHash) {
            lastHash = hash;
            if (!m_aiAnimTimer->isActive())
                m_aiAnimTimer->start();
            m_aiIdleTimer->start();
        }
    });
    pollTimer->start();

    promptLayout->addWidget(m_editor, 1);
    promptLayout->addLayout(btnLayout);

    m_bottomTabWidget->addTab(promptTab, "Prompt");

    m_bottomTerminal = new TerminalWidget;
    m_bottomTabWidget->addTab(m_bottomTerminal, "Terminal");

    m_bottomTerminal2 = new TerminalWidget;
    m_bottomTabWidget->addTab(m_bottomTerminal2, "Terminal 2");

    // Notifications tab
    m_notificationPanel = new NotificationPanel;
    m_bottomTabWidget->addTab(m_notificationPanel, "Notifications");
    connect(m_notificationPanel, &NotificationPanel::unreadChanged, this, [this](int count) {
        int idx = m_bottomTabWidget->indexOf(m_notificationPanel);
        if (count > 0)
            m_bottomTabWidget->setTabText(idx, QString("Notifications (%1)").arg(count));
        else
            m_bottomTabWidget->setTabText(idx, "Notifications");
    });

    // Diff viewer tab
    m_diffViewer = new DiffViewer;
    m_bottomTabWidget->addTab(m_diffViewer, "Diff");

    // Changes monitor — tracks file changes in project
    m_changesMonitor = new ChangesMonitor;
    m_bottomTabWidget->addTab(m_changesMonitor, "Changes");

    // Git graph — commit history visualization
    m_gitGraph = new GitGraph;
    m_bottomTabWidget->addTab(m_gitGraph, "Git");

    // Workspace search tab (created early so currentChanged handler can reference it)
    m_workspaceSearch = new WorkspaceSearch;
    m_bottomTabWidget->addTab(m_workspaceSearch, "Search");

    // Git blame tab (created early so currentChanged handler can reference it)
    m_gitBlame = new GitBlame;
    m_bottomTabWidget->addTab(m_gitBlame, "Blame");

    connect(m_gitGraph, &GitGraph::outputMessage, this, [this](const QString &msg, int level) {
        notify(msg, level);
    });

    connect(m_changesMonitor, &ChangesMonitor::changeDetected, this, [this](const QString &path) {
        QString rel = path;
        if (rel.startsWith(m_fileBrowser->rootPath()))
            rel = rel.mid(m_fileBrowser->rootPath().length() + 1);
        int idx = m_bottomTabWidget->indexOf(m_changesMonitor);
        int count = m_changesMonitor->changeCount();
        m_bottomTabWidget->setTabText(idx, QString("Changes (%1)").arg(count));
        notify("File changed: " + rel, 0);

        // Auto-reload if file is open in editor
        onFileChanged(path);
    });

    connect(m_changesMonitor, &ChangesMonitor::fileReverted, this, [this](const QString &path) {
        QString rel = path;
        if (rel.startsWith(m_fileBrowser->rootPath()))
            rel = rel.mid(m_fileBrowser->rootPath().length() + 1);
        notify("Reverted: " + rel, 3);
        onFileChanged(path);
    });

    connect(m_changesMonitor, &ChangesMonitor::openFileRequested,
            this, &MainWindow::onFileOpened);

    // Auto-refresh Diff when switching to its tab
    connect(m_bottomTabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        if (m_bottomTabWidget->widget(idx) == m_diffViewer) {
            m_diffViewer->refresh(m_fileBrowser->rootPath());
        } else if (m_bottomTabWidget->widget(idx) == m_gitGraph) {
            m_gitGraph->refresh(m_fileBrowser->rootPath());
        } else if (m_bottomTabWidget->widget(idx) == m_changesMonitor) {
            // Clear badge
            m_bottomTabWidget->setTabText(idx, "Changes");
        } else if (m_bottomTabWidget->widget(idx) == m_gitBlame) {
            // Auto-blame current file when switching to Blame tab
            blameCurrentFile();
        }
    });

    // Editor splitter wraps the main tab widget (split view adds second tab widget here)
    m_editorSplitter = new QSplitter(Qt::Horizontal);
    m_editorSplitter->addWidget(m_tabWidget);

    m_rightSplitter->addWidget(m_editorSplitter);
    m_rightSplitter->addWidget(m_bottomTabWidget);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 1);

    m_fileBrowser = new FileBrowser;
    m_fileBrowser->setSshManager(m_sshManager);

    m_mainSplitter->addWidget(m_fileBrowser);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_mainSplitter, 1);

    // Status bar: file label | SSH profile combo | transfer progress | info label
    m_statusFileLabel = new QLabel("Ready");
    m_statusFileLabel->setMinimumWidth(0);
    m_statusInfoLabel = new QLabel;
    m_statusInfoLabel->setMinimumWidth(0);
    m_sshProfileCombo = new QComboBox;
    m_sshProfileCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_sshProfileCombo->setMinimumWidth(100);
    m_sshProfileCombo->addItem("No SSH");
    m_sshProfileCombo->hide();
    m_transferProgress = new QProgressBar;
    m_transferProgress->setFixedWidth(150);
    m_transferProgress->setTextVisible(true);
    m_transferProgress->hide();

    statusBar()->setSizeGripEnabled(false);
    statusBar()->setContentsMargins(0, 0, 0, 0);
    statusBar()->setStyleSheet("QStatusBar { border: none; } QStatusBar::item { border: none; }");
    statusBar()->addWidget(m_statusFileLabel, 1);
    statusBar()->addWidget(m_statusInfoLabel, 0);
    statusBar()->addPermanentWidget(m_sshProfileCombo);
    statusBar()->addPermanentWidget(m_transferProgress);

    // File watcher with debouncing
    m_fileWatcher = new QFileSystemWatcher(this);
    m_fileChangeDebounce = new QTimer(this);
    m_fileChangeDebounce->setSingleShot(true);
    m_fileChangeDebounce->setInterval(300);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        m_pendingFileChanges.insert(path);
        m_fileChangeDebounce->start();
    });
    connect(m_fileChangeDebounce, &QTimer::timeout, this, [this]() {
        QSet<QString> paths = m_pendingFileChanges;
        m_pendingFileChanges.clear();
        for (const QString &path : paths)
            onFileChanged(path);
    });

    connect(m_fileBrowser, &FileBrowser::fileOpened, this, &MainWindow::onFileOpened);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_repeatPromptBtn, &QPushButton::clicked, this, [this]() {
        if (!m_lastPrompt.isEmpty()) {
            m_editor->setPlainText(m_lastPrompt);
        }
    });
    connect(m_gitGraph, &GitGraph::commitRequested, this, &MainWindow::onCommitClicked);
    connect(m_editor, &PromptEdit::sendRequested, this, &MainWindow::onSendClicked);
    connect(m_editor, &PromptEdit::saveAndSendRequested, this, [this]() {
        QString text = m_editor->toPlainText().trimmed();
        if (text.isEmpty()) return;
        if (m_project.isLoaded()) {
            m_project.addPrompt(text);
            int id = m_project.lastPromptId();
            m_project.addSavedPrompt(id);
            refreshSavedPrompts();
        }
        m_terminal->sendText(text);
        m_editor->clear();
        if (!m_settings.promptStayOnTab)
            m_tabWidget->setCurrentIndex(0);
    });

    // Saved prompts
    connect(m_savePromptBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_editor->toPlainText().trimmed();
        if (text.isEmpty()) return;
        if (!m_project.isLoaded()) {
            ThemedMessageBox::information(this, "Save Prompt", "No project loaded. Create a project first.");
            return;
        }
        m_project.addPrompt(text);
        int id = m_project.lastPromptId();
        m_project.addSavedPrompt(id);
        refreshSavedPrompts();
        m_statusFileLabel->setText(QString("Prompt saved (id %1)").arg(id));
    });

    connect(m_savedPromptsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index <= 0) return;
        QStringList prompts = m_project.savedPrompts();
        if (index - 1 < prompts.size())
            m_editor->setPlainText(prompts[index - 1]);
    });

    connect(deletePromptBtn, &QPushButton::clicked, this, [this]() {
        int index = m_savedPromptsCombo->currentIndex();
        if (index <= 0) return;
        m_project.removeSavedPrompt(index - 1);
        refreshSavedPrompts();
    });

    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int idx) {
        updateStatusBar();
        // Highlight the current file in the file browser
        QString filePath = (idx > 0) ? m_tabWidget->tabToolTip(idx) : QString();
        m_fileBrowser->highlightFile(filePath);
    });

    // File browser directory changes — also update changes monitor
    connect(m_fileBrowser, &FileBrowser::rootPathChanged, this, [this](const QString &path) {
        bool isSsh = m_sshManager->activeProfileIndex() >= 0;
        if (!isSsh) {
            m_changesMonitor->setProjectDir(path);
            m_gitGraph->refresh(path);
        }
        m_workspaceSearch->setProjectDir(path);
        tryLoadProject(path);
    });

    connect(m_fileBrowser, &FileBrowser::rootPathOpenedByDialog, this, [this](const QString &path) {
        if (m_sshManager->activeProfileIndex() >= 0) {
            QString remotePath = m_fileBrowser->toRemotePath(path);
            m_terminal->sendText("cd \"" + remotePath + "\"");
            m_bottomTerminal->sendText("cd \"" + remotePath + "\"");
        } else {
            m_terminal->sendText("cd \"" + path + "\"");
            m_bottomTerminal->sendText("cd \"" + path + "\"");
        }
    });

    // SSH profile combo switching
    connect(m_sshProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index <= 0) return; // "No SSH" header
        switchToSshProfile(index - 1);
    });

    // SshManager signals
    connect(m_sshManager, &SshManager::profileConnected, this, [this](int idx) {
        updateSshProfileCombo();
        m_sshDisconnectAction->setEnabled(true);
        m_sshTunnelsAction->setEnabled(true);
        m_sshUploadAction->setEnabled(true);
        m_sshDownloadAction->setEnabled(true);
        m_sshProfileCombo->show();
        m_statusFileLabel->setText(QString("SSH: %1").arg(m_sshManager->profileLabel(idx)));

        switchToSshProfile(idx);
        sshConnectTerminals(m_sshManager->profileConfig(idx));
    });

    connect(m_sshManager, &SshManager::connectFailed, this, [this](int idx, const QString &err) {
        m_sshManager->removeProfile(idx);
        m_statusFileLabel->setText("SSH connect failed");
        notify("SSH connect failed: " + err, 2);
        ThemedMessageBox::warning(this, "SSH Error", err);
    });

    connect(m_sshManager, &SshManager::profileDisconnected, this, [this](int) {
        qDebug() << "SSH profileDisconnected: updating combo";
        updateSshProfileCombo();
        if (m_sshManager->activeProfileIndex() < 0) {
            qDebug() << "SSH profileDisconnected: disabling actions";
            m_sshDisconnectAction->setEnabled(false);
            m_sshTunnelsAction->setEnabled(false);
            m_sshUploadAction->setEnabled(false);
            m_sshDownloadAction->setEnabled(false);
            m_sshProfileCombo->hide();
            qDebug() << "SSH profileDisconnected: clearing ssh mount";
            m_fileBrowser->clearSshMount();
            m_gitGraph->clearSshInfo();
            if (!m_localRootBeforeSsh.isEmpty()) {
                qDebug() << "SSH profileDisconnected: restoring local root" << m_localRootBeforeSsh;
                m_fileBrowser->setRootPath(m_localRootBeforeSsh);
            }
            qDebug() << "SSH profileDisconnected: done";
            m_statusFileLabel->setText("Ready");
        }
    });

    connect(m_sshManager, &SshManager::connectionLost, this, [this](int idx) {
        m_statusFileLabel->setText(QString("SSH LOST: %1 - reconnecting...")
                                       .arg(m_sshManager->profileLabel(idx)));
        notify("SSH connection lost: " + m_sshManager->profileLabel(idx), 1);
    });

    connect(m_sshManager, &SshManager::reconnected, this, [this](int idx) {
        m_statusFileLabel->setText(QString("SSH reconnected: %1").arg(m_sshManager->profileLabel(idx)));
        notify("SSH reconnected: " + m_sshManager->profileLabel(idx), 3);
        // Re-setup file browser if this is the active profile
        if (idx == m_sshManager->activeProfileIndex()) {
            m_fileBrowser->setSshMount(m_sshManager->mountPoint(idx), "");
            m_fileBrowser->setRootPath(m_fileBrowser->rootPath()); // refresh
        }
    });

    connect(m_sshManager, &SshManager::transferProgress, this, [this](const QString &name, int pct) {
        m_transferProgress->show();
        m_transferProgress->setValue(pct);
        m_transferProgress->setFormat(name + " %p%");
    });

    connect(m_sshManager, &SshManager::transferFinished, this,
            [this](const QString &name, bool ok, const QString &err) {
        m_transferProgress->hide();
        if (ok) {
            m_statusFileLabel->setText("Transfer complete: " + name);
            notify("Transfer complete: " + name, 3);
        } else {
            notify("Transfer failed: " + name + " - " + err, 2);
            ThemedMessageBox::warning(this, "Transfer Error", "Failed: " + name + "\n" + err);
        }
    });

    connect(m_sshManager, &SshManager::sshError, this, [this](const QString &msg) {
        m_statusFileLabel->setText("SSH Error: " + msg);
    });

    // Ctrl+S
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrentFile);

    // Ctrl+M — Markdown preview
    auto *mdShortcut = new QShortcut(QKeySequence("Ctrl+M"), this);
    connect(mdShortcut, &QShortcut::activated, this, &MainWindow::toggleMarkdownPreview);

    // Ctrl+= / Ctrl+- — zoom font size (context-dependent)
    auto zoomFocused = [this](int delta) {
        QWidget *fw = QApplication::focusWidget();
        // Editor tab
        if (auto *editor = qobject_cast<CodeEditor *>(fw)) {
            Q_UNUSED(editor);
            m_settings.editorFontSize = qBound(6, m_settings.editorFontSize + delta, 48);
            m_settings.save();
            applySettings();
            return;
        }
        // Prompt edit
        if (fw == m_editor || (fw && fw->parentWidget() == m_editor)) {
            m_settings.promptFontSize = qBound(6, m_settings.promptFontSize + delta, 48);
            m_settings.save();
            applySettings();
            return;
        }
        // File browser
        if (fw && (fw == m_fileBrowser || m_fileBrowser->isAncestorOf(fw))) {
            m_settings.browserFontSize = qBound(6, m_settings.browserFontSize + delta, 48);
            m_settings.save();
            applySettings();
            return;
        }
        // Markdown preview
        if (auto *mdp = currentMarkdownPreview()) {
            if (delta > 0) mdp->zoomIn(); else mdp->zoomOut();
            return;
        }
        // Fallback: check if current tab is a markdown preview
        for (auto *mdp : m_mdPreviews) {
            if (mdp->isAncestorOf(fw)) {
                if (delta > 0) mdp->zoomIn(); else mdp->zoomOut();
                return;
            }
        }
    };
    auto *zoomInShortcut = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(zoomInShortcut, &QShortcut::activated, this, [zoomFocused]() { zoomFocused(1); });
    auto *zoomOutShortcut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(zoomOutShortcut, &QShortcut::activated, this, [zoomFocused]() { zoomFocused(-1); });

    // Command palette
    m_commandPalette = new CommandPalette(this);
    setupCommandPalette();
    auto *paletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(paletteShortcut, &QShortcut::activated, m_commandPalette, &CommandPalette::show);

    // Fuzzy file opener (Ctrl+P)
    m_fileOpener = new FileOpener(this);
    connect(m_fileOpener, &FileOpener::fileSelected, this, &MainWindow::onFileOpened);
    auto *fileOpenerShortcut = new QShortcut(QKeySequence("Ctrl+P"), this);
    connect(fileOpenerShortcut, &QShortcut::activated, this, [this]() {
        m_fileOpener->setRootPath(m_fileBrowser->rootPath());
        m_fileOpener->show();
    });

    // Workspace search (Ctrl+Shift+F) — connects for already-created widget
    connect(m_workspaceSearch, &WorkspaceSearch::fileRequested, this, [this](const QString &path, int line) {
        onFileOpened(path);
        // Jump to line
        int idx = m_tabWidget->currentIndex();
        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(idx));
        if (editor) {
            QTextCursor cursor = editor->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        }
    });
    auto *wsSearchShortcut = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    connect(wsSearchShortcut, &QShortcut::activated, this, [this]() {
        m_workspaceSearch->setProjectDir(m_fileBrowser->rootPath());
        m_bottomTabWidget->setCurrentWidget(m_workspaceSearch);
        m_workspaceSearch->focusSearch();
    });

    // Git blame — connects for already-created widget
    connect(m_gitBlame, &GitBlame::outputMessage, this, [this](const QString &msg, int level) {
        notify(msg, level);
    });

    // Pre-initialize QWebEngine to avoid flicker on first MarkdownPreview
    auto *warmup = new QWebEngineView(this);
    warmup->setFixedSize(0, 0);
    warmup->setVisible(false);
    warmup->load(QUrl("about:blank"));
    connect(warmup, &QWebEngineView::loadFinished, warmup, &QObject::deleteLater);

    applyGlobalTheme();
    applySettings();
    restoreSession();

    // QTermWidget resets font after show/startShellProgram — reapply after event loop
    QTimer::singleShot(0, this, [this]() {
        QFont termFont;
        termFont.setFamily(m_settings.termFontFamily);
        termFont.setPointSize(m_settings.termFontSize);
        termFont.setStyleHint(QFont::Monospace);
        termFont.setFixedPitch(true);
        m_terminal->terminal()->setTerminalFont(termFont);
        m_bottomTerminal->terminal()->setTerminalFont(termFont);
    });

    m_terminal->sendText("cd \"" + m_fileBrowser->rootPath() + "\"");
    m_bottomTerminal->sendText("cd \"" + m_fileBrowser->rootPath() + "\"");
    tryLoadProject(m_fileBrowser->rootPath());
    m_changesMonitor->setProjectDir(m_fileBrowser->rootPath());
}

MainWindow::~MainWindow()
{
    m_sshManager->disconnectAll();
    saveSession();
}

// ── SSH ─────────────────────────────────────────────────────────────

void MainWindow::onSshConnect()
{
    SshConfig lastCfg;
    if (m_sshManager->activeProfileIndex() >= 0)
        lastCfg = m_sshManager->profileConfig(m_sshManager->activeProfileIndex());

    SshDialog dlg(lastCfg, this);
    dlg.setStyleSheet(styleSheet());
    if (dlg.exec() != QDialog::Accepted)
        return;

    SshConfig cfg = dlg.result();
    if (cfg.host.isEmpty() || cfg.user.isEmpty()) {
        ThemedMessageBox::warning(this, "SSH", "User and host are required.");
        return;
    }

    // Validate against command injection
    if (!SshManager::isValidSshIdentifier(cfg.user) || !SshManager::isValidSshIdentifier(cfg.host)) {
        ThemedMessageBox::warning(this, "SSH", "Invalid characters in user or host.");
        return;
    }

    // Save local root before first SSH connection
    if (m_sshManager->profileCount() == 0)
        m_localRootBeforeSsh = m_fileBrowser->rootPath();

    int idx = m_sshManager->addProfile(cfg);
    m_statusFileLabel->setText("Connecting...");
    m_sshManager->connectProfile(idx); // async — result via profileConnected/connectFailed signals
}

void MainWindow::onSshDisconnect()
{
    int idx = m_sshManager->activeProfileIndex();
    if (idx < 0) return;

    qDebug() << "SSH Disconnect: step 1 - disconnecting terminals";
    sshDisconnectTerminals();
    qDebug() << "SSH Disconnect: step 2 - disconnecting profile" << idx;
    m_sshManager->disconnectProfile(idx);
    qDebug() << "SSH Disconnect: step 3 - done";
}

void MainWindow::switchToSshProfile(int index)
{
    if (!m_sshManager->isConnected(index)) return;
    m_sshManager->setActiveProfile(index);

    const auto &cfg = m_sshManager->profileConfig(index);
    QString mp = m_sshManager->mountPoint(index);

    m_fileBrowser->setSshMount(mp, "");
    m_gitGraph->setSshInfo(m_sshManager, mp, "");
    QString startPath = cfg.remotePath;
    if (startPath == "~" || startPath.isEmpty())
        startPath = "/home/" + cfg.user;
    m_fileBrowser->setRootPath(mp + startPath);

    m_statusFileLabel->setText(QString("SSH: %1").arg(m_sshManager->profileLabel(index)));

    // Update combo without triggering signal
    m_sshProfileCombo->blockSignals(true);
    m_sshProfileCombo->setCurrentIndex(index + 1);
    m_sshProfileCombo->blockSignals(false);
}

void MainWindow::sshConnectTerminals(const SshConfig &cfg)
{
    QString sshLine = QString("ssh -o StrictHostKeyChecking=accept-new %1@%2 -p %3")
                          .arg(cfg.user, cfg.host, QString::number(cfg.port));
    if (!cfg.identityFile.isEmpty()) {
        QString quoted = cfg.identityFile;
        quoted.replace("'", "'\\''");
        sshLine += " -i '" + quoted + "'";
    }

    m_terminal->sendText(sshLine);
    m_bottomTerminal->sendText(sshLine);

    if (!cfg.password.isEmpty()) {
        // Send password directly to terminal PTY (ssh prompts with echo off,
        // so the password is not visible in terminal output)
        QString pwd = cfg.password;
        QTimer::singleShot(2000, this, [this, pwd]() {
            m_terminal->terminal()->sendText(pwd + "\r");
            m_bottomTerminal->terminal()->sendText(pwd + "\r");
        });
    }

    int cdDelay = cfg.password.isEmpty() ? 3000 : 4000;
    QString remotePath = cfg.remotePath;
    if (remotePath != "~" && !remotePath.isEmpty()) {
        QTimer::singleShot(cdDelay, this, [this, remotePath]() {
            m_terminal->sendText("cd \"" + remotePath + "\"");
            m_bottomTerminal->sendText("cd \"" + remotePath + "\"");
        });
    }
}

void MainWindow::sshDisconnectTerminals()
{
    m_terminal->sendText("exit");
    m_bottomTerminal->sendText("exit");
}

void MainWindow::updateSshProfileCombo()
{
    m_sshProfileCombo->blockSignals(true);
    m_sshProfileCombo->clear();
    m_sshProfileCombo->addItem("No SSH");

    for (int i = 0; i < m_sshManager->profileCount(); ++i) {
        QString label = m_sshManager->profileLabel(i);
        if (m_sshManager->isConnected(i))
            label += " [connected]";
        m_sshProfileCombo->addItem(label);
    }

    int active = m_sshManager->activeProfileIndex();
    m_sshProfileCombo->setCurrentIndex(active >= 0 ? active + 1 : 0);
    m_sshProfileCombo->blockSignals(false);
}

void MainWindow::onSshTunnels()
{
    int idx = m_sshManager->activeProfileIndex();
    if (idx < 0) {
        ThemedMessageBox::information(this, "SSH Tunnels", "No active SSH connection.");
        return;
    }
    SshTunnelDialog dlg(m_sshManager, idx, this);
    dlg.setStyleSheet(styleSheet());
    dlg.exec();
}

void MainWindow::onSshUpload()
{
    int idx = m_sshManager->activeProfileIndex();
    if (idx < 0) return;

    QString localFile = QFileDialog::getOpenFileName(this, "Select file to upload");
    if (localFile.isEmpty()) return;

    // Upload to current remote directory
    QString remotePath = m_fileBrowser->toRemotePath(m_fileBrowser->rootPath());
    remotePath += "/" + QFileInfo(localFile).fileName();

    m_sshManager->uploadFile(idx, localFile, remotePath);
}

void MainWindow::onSshDownload()
{
    int idx = m_sshManager->activeProfileIndex();
    if (idx < 0) return;

    // Ask for remote path
    bool ok;
    QString remotePath = QInputDialog::getText(this, "Download File",
        "Remote file path:", QLineEdit::Normal,
        m_fileBrowser->toRemotePath(m_fileBrowser->rootPath()) + "/", &ok);
    if (!ok || remotePath.isEmpty()) return;

    QString localFile = QFileDialog::getSaveFileName(this, "Save as",
        QDir::homePath() + "/" + QFileInfo(remotePath).fileName());
    if (localFile.isEmpty()) return;

    m_sshManager->downloadFile(idx, remotePath, localFile);
}

// ── Non-SSH methods (unchanged) ─────────────────────────────────────

void MainWindow::applySettingsToEditor(CodeEditor *editor, const QString &lang)
{
    QFont font(m_settings.editorFontFamily, m_settings.editorFontSize);
    font.setWeight(static_cast<QFont::Weight>(m_settings.editorFontWeight));
    editor->setFont(font);
    editor->document()->setDefaultFont(font);
    editor->setShowLineNumbers(m_settings.showLineNumbers);

    // Set language first (no rehighlight), then color scheme triggers single rehighlight
    if (m_settings.syntaxHighlighting && !lang.isEmpty()) {
        editor->highlighter()->setLanguage(lang);
    } else {
        editor->highlighter()->setLanguage("");
    }
    editor->setEditorColorScheme(m_settings.editorColorScheme, m_settings.bgColor, m_settings.textColor);
    {
        const ExternalTheme *et = m_settings.findExternalTheme(m_settings.globalTheme);
        if (et && !et->lineHighlight.isEmpty())
            editor->setLineHighlightColor(QColor(et->lineHighlight));
    }
    editor->setHighlightCurrentLine(m_settings.editorHighlightLine);
    editor->setLineSpacing(m_settings.editorLineSpacing);
}

void MainWindow::updateStatusBar()
{
    int idx = m_tabWidget->currentIndex();
    if (idx == 0) {
        m_statusFileLabel->setText("AI-terminal");
        m_statusInfoLabel->clear();
    } else {
        QString filePath = m_tabWidget->tabToolTip(idx);
        if (m_sshManager->activeProfileIndex() >= 0)
            m_statusFileLabel->setText(m_fileBrowser->toRemotePath(filePath));
        else
            m_statusFileLabel->setText(filePath);
        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(idx));
        if (editor) {
            int line = editor->textCursor().blockNumber() + 1;
            int col = editor->textCursor().columnNumber() + 1;
            int lines = editor->blockCount();
            m_statusInfoLabel->setText(
                QString("Ln %1, Col %2  |  %3 lines").arg(line).arg(col).arg(lines));
        }
    }
}

void MainWindow::applySettings()
{
    // Widget style (Fusion, Windows, etc.)
    if (m_settings.widgetStyle != "Auto") {
        QStyle *style = QStyleFactory::create(m_settings.widgetStyle);
        if (style) qApp->setStyle(style);
    }

    // GUI font for tabs, buttons, status bar, menus
    QFont guiFont(m_settings.guiFontFamily, m_settings.guiFontSize);
    guiFont.setWeight(static_cast<QFont::Weight>(m_settings.guiFontWeight));
    m_tabWidget->setFont(guiFont);
    m_bottomTabWidget->setFont(guiFont);
    m_sendBtn->setFont(guiFont);
    m_stopBtn->setFont(guiFont);
    m_savePromptBtn->setFont(guiFont);
    m_repeatPromptBtn->setFont(guiFont);
    m_savedPromptsCombo->setFont(guiFont);
    statusBar()->setFont(guiFont);
    m_statusFileLabel->setFont(guiFont);
    m_statusInfoLabel->setFont(guiFont);
    if (m_menuBtn) m_menuBtn->setFont(guiFont);

    QFont termFont;
    termFont.setFamily(m_settings.termFontFamily);
    termFont.setPointSize(m_settings.termFontSize);
    termFont.setWeight(static_cast<QFont::Weight>(m_settings.termFontWeight));
    termFont.setStyleHint(QFont::Monospace);
    termFont.setFixedPitch(true);
    m_terminal->terminal()->setTerminalFont(termFont);
    m_terminal->terminal()->setColorScheme(m_settings.terminalColorScheme);
    m_bottomTerminal->terminal()->setTerminalFont(termFont);
    m_bottomTerminal->terminal()->setColorScheme(m_settings.terminalColorScheme);
    m_bottomTerminal2->terminal()->setTerminalFont(termFont);
    m_bottomTerminal2->terminal()->setColorScheme(m_settings.terminalColorScheme);

    QFont promptFont(m_settings.promptFontFamily, m_settings.promptFontSize);
    promptFont.setWeight(static_cast<QFont::Weight>(m_settings.promptFontWeight));
    m_editor->setFont(promptFont);
    m_editor->setStyleSheet(
        QString("QPlainTextEdit { background-color: %1; color: %2; font-family: '%3'; font-size: %4pt; }")
            .arg(m_settings.bgColor.name())
            .arg(m_settings.textColor.name())
            .arg(m_settings.promptFontFamily)
            .arg(m_settings.promptFontSize));
    m_editor->setSendOnEnter(m_settings.promptSendKey == "Enter");
    {
        const ExternalTheme *et = m_settings.findExternalTheme(m_settings.globalTheme);
        if (et && !et->lineHighlight.isEmpty())
            m_editor->setLineHighlightColor(QColor(et->lineHighlight));
    }
    m_editor->setHighlightCurrentLine(m_settings.promptHighlightLine);

    QFont browserFont(m_settings.browserFontFamily, m_settings.browserFontSize);
    browserFont.setWeight(static_cast<QFont::Weight>(m_settings.browserFontWeight));
    m_fileBrowser->setFont(browserFont);
    m_fileBrowser->setTheme(m_settings.browserTheme, m_settings.bgColor, m_settings.textColor);
    {
        const ExternalTheme *et = m_settings.findExternalTheme(m_settings.globalTheme);
        if (et) {
            m_fileBrowser->setThemeColors(
                QColor(et->hoverBg), QColor(et->selectedBg),
                et->lineHighlight.isEmpty() ? QColor() : QColor(et->lineHighlight));
        }
    }
    m_fileBrowser->setGitignoreVisibility(m_settings.gitignoreVisibility);
    m_fileBrowser->setDotGitVisibility(m_settings.dotGitVisibility);

    // Diff viewer
    QFont diffFont(m_settings.diffFontFamily, m_settings.diffFontSize);
    diffFont.setWeight(static_cast<QFont::Weight>(m_settings.diffFontWeight));
    m_diffViewer->setViewerFont(diffFont);
    m_diffViewer->setViewerColors(m_settings.bgColor, m_settings.textColor);

    // Changes monitor
    QFont changesFont(m_settings.changesFontFamily, m_settings.changesFontSize);
    changesFont.setWeight(static_cast<QFont::Weight>(m_settings.changesFontWeight));
    m_changesMonitor->setViewerFont(changesFont);
    m_changesMonitor->setViewerColors(m_settings.bgColor, m_settings.textColor);

    // Git graph
    QFont gitGraphFont(m_settings.diffFontFamily, m_settings.diffFontSize);
    gitGraphFont.setWeight(static_cast<QFont::Weight>(m_settings.diffFontWeight));
    m_gitGraph->setViewerFont(gitGraphFont);
    m_gitGraph->setViewerColors(m_settings.bgColor, m_settings.textColor);

    // Workspace search
    m_workspaceSearch->setViewerFont(diffFont);
    m_workspaceSearch->setViewerColors(m_settings.bgColor, m_settings.textColor);

    // Git blame
    m_gitBlame->setViewerFont(diffFont);
    m_gitBlame->setViewerColors(m_settings.bgColor, m_settings.textColor);

    // Markdown previews — update font
    for (auto *mdp : m_mdPreviews) {
        QFont previewFont(m_settings.editorFontFamily, m_settings.editorFontSize);
        mdp->setFont(previewFont);
    }

    for (int i = 1; i < m_tabWidget->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
        if (editor) {
            QString filePath = m_tabWidget->tabToolTip(i);
            QString lang = langFromSuffix(QFileInfo(filePath).suffix());
            applySettingsToEditor(editor, lang);
        }
    }
    if (m_splitTabWidget) {
        for (int i = 0; i < m_splitTabWidget->count(); ++i) {
            auto *editor = qobject_cast<CodeEditor *>(m_splitTabWidget->widget(i));
            if (editor) {
                QString filePath = m_splitTabWidget->tabToolTip(i);
                QString lang = langFromSuffix(QFileInfo(filePath).suffix());
                applySettingsToEditor(editor, lang);
            }
        }
    }
}

void MainWindow::onSettingsTriggered()
{
    SettingsDialog dlg(m_settings, this);
    dlg.setStyleSheet(styleSheet());
    if (dlg.exec() == QDialog::Accepted) {
        m_settings = dlg.result();
        m_settings.save();
        applyGlobalTheme();
        applySettings();
    }
}

void MainWindow::onFileOpened(const QString &filePath)
{
    // Check main tabs
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == filePath) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }
    // Check split tabs
    if (m_splitTabWidget) {
        for (int i = 0; i < m_splitTabWidget->count(); ++i) {
            if (m_splitTabWidget->tabToolTip(i) == filePath) {
                m_splitTabWidget->setCurrentIndex(i);
                return;
            }
        }
    }

    QFileInfo info(filePath);

    static const qint64 MAX_FILE_SIZE = 10 * 1024 * 1024;
    if (info.size() > MAX_FILE_SIZE) {
        auto reply = ThemedMessageBox::question(this, "Large File",
            QString("'%1' is %2 MB. Opening large files may be slow.\n\nOpen anyway?")
                .arg(info.fileName())
                .arg(info.size() / (1024.0 * 1024.0), 0, 'f', 1),
            ThemedMessageBox::Yes | ThemedMessageBox::No, ThemedMessageBox::No);
        if (reply != ThemedMessageBox::Yes) return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString content = in.readAll();

    QString lang = langFromSuffix(info.suffix());

    auto *editor = new CodeEditor;
    // Disable syntax highlighting for large files (>1MB) for performance
    if (info.size() > 1024 * 1024)
        editor->setLargeFile(true);
    editor->setPlainText(content);
    applySettingsToEditor(editor, lang);

    int idx = m_tabWidget->addTab(editor, info.fileName());
    m_tabWidget->setTabToolTip(idx, filePath);
    m_tabWidget->setCurrentIndex(idx);

    // Add preview button for markdown files
    QString sfx = info.suffix().toLower();
    if (sfx == "md" || sfx == "markdown" || sfx == "mkd" || sfx == "mdx") {
        auto *previewBtn = new QToolButton;
        previewBtn->setText("\xF0\x9F\x91\x81");  // 👁 eye emoji
        previewBtn->setToolTip("Markdown Preview");
        previewBtn->setAutoRaise(true);
        previewBtn->setFixedSize(20, 20);
        previewBtn->setStyleSheet("QToolButton { border: none; padding: 0; font-size: 12px; }");
        connect(previewBtn, &QToolButton::clicked, this, [this, editor]() {
            // Switch to editor tab first so toggleMarkdownPreview finds it
            for (int i = 0; i < m_tabWidget->count(); ++i) {
                if (m_tabWidget->widget(i) == editor) {
                    m_tabWidget->setCurrentIndex(i);
                    break;
                }
            }
            toggleMarkdownPreview();
        });
        m_tabWidget->tabBar()->setTabButton(idx, QTabBar::LeftSide, previewBtn);
    }

    m_fileWatcher->addPath(filePath);



    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);

    // Track modification — mark tab with ● when content differs from saved
    editor->document()->setModified(false);
    editor->setProperty("savedContent", editor->toPlainText());
    connect(editor->document(), &QTextDocument::contentsChanged, this, [this, editor]() {
        QString saved = editor->property("savedContent").toString();
        bool dirty = (editor->toPlainText() != saved);
        editor->document()->setModified(dirty);
        for (int i = 1; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == editor) {
                QString name = QFileInfo(m_tabWidget->tabToolTip(i)).fileName();
                m_tabWidget->setTabText(i, dirty ? "● " + name : name);
                return;
            }
        }
        if (m_splitTabWidget) {
            for (int i = 0; i < m_splitTabWidget->count(); ++i) {
                if (m_splitTabWidget->widget(i) == editor) {
                    QString name = QFileInfo(m_splitTabWidget->tabToolTip(i)).fileName();
                    m_splitTabWidget->setTabText(i, dirty ? "● " + name : name);
                    return;
                }
            }
        }
    });

    updateStatusBar();
}

void MainWindow::saveCurrentFile()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 1) return;

    auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(idx));
    if (!editor) return;

    QString filePath = m_tabWidget->tabToolTip(idx);
    if (filePath.isEmpty()) return;

    m_fileWatcher->removePath(filePath);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << editor->toPlainText();
        if (out.status() != QTextStream::Ok) {
            ThemedMessageBox::warning(this, "Save Error",
                QString("Failed to write to '%1'.").arg(filePath));
        }
        file.close();
        editor->setProperty("savedContent", editor->toPlainText());
        editor->document()->setModified(false);
        m_tabWidget->setTabText(idx, QFileInfo(filePath).fileName());
        m_statusFileLabel->setText(QString("Saved: %1").arg(filePath));
    } else {
        ThemedMessageBox::warning(this, "Save Error",
            QString("Cannot open '%1' for writing:\n%2").arg(filePath, file.errorString()));
    }

    m_fileWatcher->addPath(filePath);
}

bool MainWindow::maybeSaveTab(QTabWidget *tabWidget, int index)
{
    auto *editor = qobject_cast<CodeEditor *>(tabWidget->widget(index));
    if (!editor || !editor->document()->isModified())
        return true;

    QString filePath = tabWidget->tabToolTip(index);
    QString fileName = QFileInfo(filePath).fileName();

    auto reply = ThemedMessageBox::question(this, "Unsaved Changes",
        QString("'%1' has unsaved changes.\n\nDo you want to save before closing?").arg(fileName),
        ThemedMessageBox::Save | ThemedMessageBox::Discard | ThemedMessageBox::Cancel,
        ThemedMessageBox::Save);

    if (reply == ThemedMessageBox::Cancel)
        return false;

    if (reply == ThemedMessageBox::Save) {
        if (filePath.isEmpty())
            return false;
        m_fileWatcher->removePath(filePath);
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << editor->toPlainText();
            file.close();
            editor->setProperty("savedContent", editor->toPlainText());
            editor->document()->setModified(false);
        } else {
            ThemedMessageBox::warning(this, "Save Error",
                QString("Cannot save '%1':\n%2").arg(filePath, file.errorString()));
            return false;
        }
        m_fileWatcher->addPath(filePath);
    }

    return true; // Discard or saved successfully
}

bool MainWindow::hasUnsavedChanges()
{
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
        if (editor && editor->document()->isModified())
            return true;
    }
    if (m_splitTabWidget) {
        for (int i = 0; i < m_splitTabWidget->count(); ++i) {
            auto *editor = qobject_cast<CodeEditor *>(m_splitTabWidget->widget(i));
            if (editor && editor->document()->isModified())
                return true;
        }
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (hasUnsavedChanges()) {
        auto reply = ThemedMessageBox::question(this, "Unsaved Changes",
            "You have unsaved changes. What do you want to do?",
            ThemedMessageBox::SaveAll | ThemedMessageBox::Discard | ThemedMessageBox::Cancel,
            ThemedMessageBox::SaveAll);

        if (reply == ThemedMessageBox::Cancel) {
            event->ignore();
            return;
        }

        if (reply == ThemedMessageBox::SaveAll) {
            // Save all modified tabs
            for (int i = 1; i < m_tabWidget->count(); ++i) {
                auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
                if (editor && editor->document()->isModified()) {
                    QString filePath = m_tabWidget->tabToolTip(i);
                    if (!filePath.isEmpty()) {
                        QFile file(filePath);
                        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            QTextStream out(&file);
                            out << editor->toPlainText();
                            file.close();
                            editor->document()->setModified(false);
                        }
                    }
                }
            }
            if (m_splitTabWidget) {
                for (int i = 0; i < m_splitTabWidget->count(); ++i) {
                    auto *editor = qobject_cast<CodeEditor *>(m_splitTabWidget->widget(i));
                    if (editor && editor->document()->isModified()) {
                        QString filePath = m_splitTabWidget->tabToolTip(i);
                        if (!filePath.isEmpty()) {
                            QFile file(filePath);
                            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                QTextStream out(&file);
                                out << editor->toPlainText();
                                file.close();
                                editor->document()->setModified(false);
                            }
                        }
                    }
                }
            }
        }
        // Discard: just continue closing
    }

    event->accept();
}

bool MainWindow::event(QEvent *event)
{
    // Enable native resize edges for frameless window
    if (event->type() == QEvent::HoverMove && !isMaximized()) {
        auto *he = static_cast<QHoverEvent *>(event);
        const int margin = 5;
        QPoint pos = he->position().toPoint();
        Qt::Edges edges;
        if (pos.x() < margin) edges |= Qt::LeftEdge;
        if (pos.x() > width() - margin) edges |= Qt::RightEdge;
        if (pos.y() < margin) edges |= Qt::TopEdge;
        if (pos.y() > height() - margin) edges |= Qt::BottomEdge;

        if (edges) {
            if (edges == Qt::LeftEdge || edges == Qt::RightEdge)
                setCursor(Qt::SizeHorCursor);
            else if (edges == Qt::TopEdge || edges == Qt::BottomEdge)
                setCursor(Qt::SizeVerCursor);
            else if ((edges & Qt::TopEdge) && (edges & Qt::LeftEdge))
                setCursor(Qt::SizeFDiagCursor);
            else if ((edges & Qt::BottomEdge) && (edges & Qt::RightEdge))
                setCursor(Qt::SizeFDiagCursor);
            else
                setCursor(Qt::SizeBDiagCursor);
        } else {
            unsetCursor();
        }
    }
    if (event->type() == QEvent::MouseButtonPress && !isMaximized()) {
        auto *me = static_cast<QMouseEvent *>(event);
        const int margin = 5;
        QPoint pos = me->pos();
        Qt::Edges edges;
        if (pos.x() < margin) edges |= Qt::LeftEdge;
        if (pos.x() > width() - margin) edges |= Qt::RightEdge;
        if (pos.y() < margin) edges |= Qt::TopEdge;
        if (pos.y() > height() - margin) edges |= Qt::BottomEdge;

        if (edges) {
            if (auto *win = windowHandle())
                win->startSystemResize(edges);
            return true;
        }
    }
    return QMainWindow::event(event);
}

void MainWindow::closeTab(QTabWidget *tabWidget, int index)
{
    if (index == 0) return; // never close AI-terminal
    QWidget *w = tabWidget->widget(index);
    // Handle markdown preview tab — delete it
    if (auto *mdp = qobject_cast<MarkdownPreview *>(w)) {
        tabWidget->removeTab(index);
        m_mdPreviews.removeOne(mdp);
        mdp->deleteLater();
        return;
    }
    if (!maybeSaveTab(tabWidget, index)) return;
    QString path = tabWidget->tabToolTip(index);
    if (!path.isEmpty())
        m_fileWatcher->removePath(path);
    tabWidget->removeTab(index);
    w->deleteLater();
}

void MainWindow::closeOtherTabs(QTabWidget *tabWidget, int keepIndex)
{
    // Close tabs after keepIndex first (reverse), then before
    for (int i = tabWidget->count() - 1; i > keepIndex; --i)
        closeTab(tabWidget, i);
    for (int i = keepIndex - 1; i >= 1; --i)
        closeTab(tabWidget, i);
}

void MainWindow::closeAllTabs(QTabWidget *tabWidget)
{
    for (int i = tabWidget->count() - 1; i >= 1; --i)
        closeTab(tabWidget, i);
}

void MainWindow::closeTabsToTheRight(QTabWidget *tabWidget, int fromIndex)
{
    for (int i = tabWidget->count() - 1; i > fromIndex; --i)
        closeTab(tabWidget, i);
}

void MainWindow::closeTabsToTheLeft(QTabWidget *tabWidget, int fromIndex)
{
    for (int i = fromIndex - 1; i >= 1; --i)
        closeTab(tabWidget, i);
}

void MainWindow::onSendClicked()
{
    QString text = m_editor->toPlainText();
    if (!text.isEmpty()) {
        m_lastPrompt = text;
        m_repeatPromptBtn->setEnabled(true);
        if (m_project.isLoaded())
            m_project.addPrompt(text);
        m_terminal->sendText(text);
        m_editor->clear();
        if (!m_settings.promptStayOnTab)
            m_tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::onStopClicked()
{
    QString seq = m_settings.modelStopSequence;
    // Parse escape sequences: \x03 -> Ctrl+C, \x1b -> Escape, \n -> newline
    QString resolved;
    for (int i = 0; i < seq.size(); ++i) {
        if (seq[i] == '\\' && i + 1 < seq.size()) {
            QChar next = seq[i + 1];
            if (next == 'x' && i + 3 < seq.size()) {
                bool ok;
                int code = seq.mid(i + 2, 2).toInt(&ok, 16);
                if (ok) {
                    resolved += QChar(code);
                    i += 3;
                    continue;
                }
            } else if (next == 'n') {
                resolved += '\n';
                ++i;
                continue;
            } else if (next == 't') {
                resolved += '\t';
                ++i;
                continue;
            } else if (next == '\\') {
                resolved += '\\';
                ++i;
                continue;
            }
        }
        resolved += seq[i];
    }
    m_terminal->sendText(resolved);
}

void MainWindow::onCommitClicked()
{
    QString dir = m_fileBrowser->rootPath();

    // Show commit message dialog
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Commit");
    dlg->setMinimumWidth(450);
    auto *layout = new QVBoxLayout(dlg);

    auto *label = new QLabel("Commit message:");
    layout->addWidget(label);

    auto *msgEdit = new QLineEdit;
    QString defaultMsg = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    msgEdit->setText(defaultMsg);
    msgEdit->selectAll();
    layout->addWidget(msgEdit);

    auto *btnLayout2 = new QHBoxLayout;
    auto *okBtn = new QPushButton("OK");
    auto *cancelBtn = new QPushButton("Cancel");
    btnLayout2->addStretch();
    btnLayout2->addWidget(okBtn);
    btnLayout2->addWidget(cancelBtn);
    layout->addLayout(btnLayout2);

    connect(okBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);
    connect(msgEdit, &QLineEdit::returnPressed, dlg, &QDialog::accept);

    ThemedDialog::apply(dlg, "Commit");

    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }

    QString commitMsg = msgEdit->text().trimmed();
    if (commitMsg.isEmpty())
        commitMsg = defaultMsg;
    dlg->deleteLater();

    m_statusFileLabel->setText("Committing...");

    // Run git operations async in a chain
    auto *git = new QProcess(this);
    git->setWorkingDirectory(dir);
    auto output = std::make_shared<QString>();

    // Cleanup helper — always deletes git process, even on error
    auto cleanup = [this, git, output, commitMsg]() {
        QString out = output->trimmed();
        m_statusFileLabel->setText(QString("Committed: %1").arg(commitMsg));
        notify("Git commit: " + commitMsg, 3);
        if (!out.isEmpty())
            m_statusFileLabel->setText(out.left(100));
        m_gitGraph->refresh(m_fileBrowser->rootPath());
        git->deleteLater();
    };

    // Error handler for all steps
    connect(git, &QProcess::errorOccurred, this, [this, git, output](QProcess::ProcessError) {
        QString err = git->errorString();
        m_statusFileLabel->setText("Commit error: " + err);
        notify("Git commit failed: " + err, 2);
        git->deleteLater();
    });

    // Step 1: ensure .gitignore has sensitive file patterns
    static const QStringList sensitivePatterns = {".env", ".env.*", "*.pem", "*.key",
        "credentials.json", "id_rsa", "id_ed25519"};
    QString gitignorePath = dir + "/.gitignore";
    QString giContent;
    if (QFile::exists(gitignorePath)) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::ReadOnly)) { giContent = gi.readAll(); gi.close(); }
    }
    bool giChanged = false;
    for (const auto &pat : sensitivePatterns) {
        if (!giContent.contains(pat)) {
            if (!giContent.isEmpty() && !giContent.endsWith('\n'))
                giContent += '\n';
            giContent += pat + '\n';
            giChanged = true;
        }
    }
    if (giChanged) {
        QFile gi(gitignorePath);
        if (gi.open(QIODevice::WriteOnly | QIODevice::Text)) {
            gi.write(giContent.toUtf8());
            gi.close();
        }
    }

    // Lambda chain: init → add → commit
    auto doAdd = [this, git, output, dir, commitMsg, cleanup]() {
        disconnect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, nullptr);
        connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, git, output, dir, commitMsg, cleanup](int, QProcess::ExitStatus) {
            *output += git->readAllStandardOutput() + git->readAllStandardError();
            // Step 3: commit
            disconnect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, nullptr);
            connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, git, output, cleanup](int, QProcess::ExitStatus) {
                *output += git->readAllStandardOutput() + git->readAllStandardError();
                cleanup();
            });
            git->start("git", {"commit", "-m", commitMsg});
        });
        git->start("git", {"add", "."});
    };

    if (!QDir(dir + "/.git").exists()) {
        connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, git, output, dir, doAdd](int, QProcess::ExitStatus) {
            *output += git->readAllStandardOutput() + git->readAllStandardError();
            disconnect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, nullptr);

            if (m_project.isLoaded() && !m_project.gitRemote().isEmpty()) {
                connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [git, output, doAdd](int, QProcess::ExitStatus) {
                    *output += git->readAllStandardOutput() + git->readAllStandardError();
                    disconnect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), nullptr, nullptr);
                    doAdd();
                });
                git->start("git", {"remote", "add", "origin", m_project.gitRemote()});
            } else {
                doAdd();
            }
        });
        git->start("git", {"init"});
    } else {
        doAdd();
    }
}

void MainWindow::onCreateProject()
{
    QString dir = m_fileBrowser->rootPath();

    if (QDir(dir + "/.LLM").exists()) {
        ThemedMessageBox::information(this, "Project",
            "Project already exists. Use 'Edit Project' to modify.");
        tryLoadProject(dir);
        return;
    }

    ProjectConfig cfg;
    cfg.name = QFileInfo(dir).fileName();
    cfg.model = "claude-opus-4-6";

    ProjectDialog dlg("Create Project", cfg, this);
    dlg.setStyleSheet(styleSheet());
    if (dlg.exec() != QDialog::Accepted)
        return;

    cfg = dlg.result();
    if (cfg.name.isEmpty()) {
        ThemedMessageBox::warning(this, "Error", "Project name is required.");
        return;
    }

    if (m_project.create(dir, cfg.name, cfg.description, cfg.model, cfg.gitRemote)) {
        m_statusFileLabel->setText(
            QString("Project: %1 [%2]").arg(cfg.name, cfg.model));
    } else {
        ThemedMessageBox::warning(this, "Error", "Failed to create project.");
    }
}

void MainWindow::onEditProject()
{
    if (!m_project.isLoaded()) {
        ThemedMessageBox::warning(this, "No Project",
            "No project loaded. Create a project first.");
        return;
    }

    ProjectConfig cfg;
    cfg.name = m_project.projectName();
    cfg.description = m_project.description();
    cfg.model = m_project.model();
    cfg.gitRemote = m_project.gitRemote();

    ProjectDialog dlg("Edit Project", cfg, this);
    dlg.setStyleSheet(styleSheet());
    if (dlg.exec() != QDialog::Accepted)
        return;

    cfg = dlg.result();
    m_project.update(cfg.name, cfg.description, cfg.model, cfg.gitRemote);

    if (!cfg.gitRemote.isEmpty()) {
        QString dir = m_fileBrowser->rootPath();
        if (QDir(dir + "/.git").exists()) {
            auto *git = new QProcess(this);
            git->setWorkingDirectory(dir);
            connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [git, dir, remote = cfg.gitRemote](int exitCode, QProcess::ExitStatus) {
                if (exitCode != 0) {
                    auto *git2 = new QProcess(git->parent());
                    git2->setWorkingDirectory(dir);
                    connect(git2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                            git2, &QObject::deleteLater);
                    git2->start("git", {"remote", "add", "origin", remote});
                }
                git->deleteLater();
            });
            git->start("git", {"remote", "set-url", "origin", cfg.gitRemote});
        }
    }

    m_statusFileLabel->setText(
        QString("Project: %1 [%2]").arg(cfg.name, cfg.model));
}

void MainWindow::onFileChanged(const QString &path)
{
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == path) {
            auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
            if (!editor) break;

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                break;

            QTextStream in(&file);
            QString content = in.readAll();

            if (editor->toPlainText() == content) break;

            int cursorPos = editor->textCursor().position();

            // Set savedContent BEFORE setPlainText — because setPlainText
            // triggers contentsChanged synchronously which compares to savedContent
            editor->setProperty("savedContent", content);
            editor->setPlainText(content);
            editor->document()->setModified(false);
            QString name = QFileInfo(path).fileName();
            m_tabWidget->setTabText(i, name);

            QTextCursor cursor = editor->textCursor();
            cursor.setPosition(qMin(cursorPos, editor->document()->characterCount() - 1));
            editor->setTextCursor(cursor);

            if (!m_fileWatcher->files().contains(path))
                m_fileWatcher->addPath(path);

            break;
        }
    }
}

void MainWindow::tryLoadProject(const QString &path)
{
    if (m_project.load(path)) {
        m_statusFileLabel->setText(
            QString("Project: %1 [%2]").arg(m_project.projectName(), m_project.model()));
        refreshSavedPrompts();
    }
}

// ── Notifications ───────────────────────────────────────────────────

void MainWindow::notify(const QString &msg, int level)
{
    switch (level) {
    case 1: m_notificationPanel->addWarning(msg); break;
    case 2: m_notificationPanel->addError(msg); break;
    case 3: m_notificationPanel->addSuccess(msg); break;
    default: m_notificationPanel->addInfo(msg); break;
    }
}

// ── Split View ──────────────────────────────────────────────────────

void MainWindow::splitEditorHorizontal()
{
    if (m_splitTabWidget) return;

    m_editorSplitter->setOrientation(Qt::Vertical);

    m_splitTabWidget = new QTabWidget;
    m_splitTabWidget->setTabsClosable(true);
    connect(m_splitTabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (!maybeSaveTab(m_splitTabWidget, index)) return;
        QWidget *w = m_splitTabWidget->widget(index);
        QString path = m_splitTabWidget->tabToolTip(index);
        if (!path.isEmpty())
            m_fileWatcher->removePath(path);
        m_splitTabWidget->removeTab(index);
        w->deleteLater();
        if (m_splitTabWidget->count() == 0)
            unsplitEditor();
    });

    m_editorSplitter->addWidget(m_splitTabWidget);

    // Move current file to split pane if one is open
    int idx = m_tabWidget->currentIndex();
    if (idx >= 1) {
        QString filePath = m_tabWidget->tabToolTip(idx);
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString lang = langFromSuffix(QFileInfo(filePath).suffix());
                auto *editor = new CodeEditor;
                editor->setPlainText(in.readAll());
                applySettingsToEditor(editor, lang);
                int newIdx = m_splitTabWidget->addTab(editor, QFileInfo(filePath).fileName());
                m_splitTabWidget->setTabToolTip(newIdx, filePath);
                connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
            }
        }
    }
}

void MainWindow::splitEditorVertical()
{
    if (m_splitTabWidget) return;

    m_editorSplitter->setOrientation(Qt::Horizontal);

    m_splitTabWidget = new QTabWidget;
    m_splitTabWidget->setTabsClosable(true);
    connect(m_splitTabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (!maybeSaveTab(m_splitTabWidget, index)) return;
        QWidget *w = m_splitTabWidget->widget(index);
        QString path = m_splitTabWidget->tabToolTip(index);
        if (!path.isEmpty())
            m_fileWatcher->removePath(path);
        m_splitTabWidget->removeTab(index);
        w->deleteLater();
        if (m_splitTabWidget->count() == 0)
            unsplitEditor();
    });

    m_editorSplitter->addWidget(m_splitTabWidget);

    int idx = m_tabWidget->currentIndex();
    if (idx >= 1) {
        QString filePath = m_tabWidget->tabToolTip(idx);
        if (!filePath.isEmpty()) {
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString lang = langFromSuffix(QFileInfo(filePath).suffix());
                auto *editor = new CodeEditor;
                editor->setPlainText(in.readAll());
                applySettingsToEditor(editor, lang);
                int newIdx = m_splitTabWidget->addTab(editor, QFileInfo(filePath).fileName());
                m_splitTabWidget->setTabToolTip(newIdx, filePath);
                connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
            }
        }
    }
}

void MainWindow::unsplitEditor()
{
    if (!m_splitTabWidget) return;

    // Close all tabs in split pane
    while (m_splitTabWidget->count() > 0) {
        QWidget *w = m_splitTabWidget->widget(0);
        m_splitTabWidget->removeTab(0);
        w->deleteLater();
    }

    m_splitTabWidget->deleteLater();
    m_splitTabWidget = nullptr;
}

// ── Markdown Preview ────────────────────────────────────────────────

void MainWindow::toggleMarkdownPreview()
{
    // Get current editor
    auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->currentWidget());
    if (!editor) return;

    QString filePath = m_tabWidget->tabToolTip(m_tabWidget->currentIndex());
    QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix != "md" && suffix != "markdown" && suffix != "mkd" && suffix != "mdx") {
        notify("Markdown preview is only available for .md files", 1);
        return;
    }

    // If a preview for this editor already exists, close it (toggle off)
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *mdp = qobject_cast<MarkdownPreview *>(m_tabWidget->widget(i));
        if (mdp && mdp->property("sourceEditor").value<QWidget *>() == editor) {
            m_tabWidget->removeTab(i);
            m_mdPreviews.removeOne(mdp);
            mdp->deleteLater();
            return;
        }
    }

    // Create a new preview for this file
    auto *preview = new MarkdownPreview(this);
    preview->setProperty("sourceEditor", QVariant::fromValue<QWidget *>(editor));

    bool dark = m_settings.globalTheme.contains("Dark") || m_settings.globalTheme == "Monokai" || m_settings.globalTheme == "Nord";
    preview->setDarkMode(dark);
    QFont previewFont(m_settings.editorFontFamily, m_settings.editorFontSize);
    preview->setFont(previewFont);

    QString tabName = "Preview: " + QFileInfo(filePath).fileName();
    int idx = m_tabWidget->addTab(preview, tabName);
    m_tabWidget->setCurrentIndex(idx);
    m_mdPreviews.append(preview);

    // Initial content
    preview->updateContent(editor->toPlainText());

    // Live updates
    connect(editor, &QPlainTextEdit::textChanged, preview, [preview, editor]() {
        preview->updateContent(editor->toPlainText());
    });
    // Clean up if source editor is destroyed
    connect(editor, &QObject::destroyed, preview, [this, preview]() {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == preview) {
                m_tabWidget->removeTab(i);
                break;
            }
        }
        m_mdPreviews.removeOne(preview);
        preview->deleteLater();
    });
}

MarkdownPreview *MainWindow::currentMarkdownPreview()
{
    return qobject_cast<MarkdownPreview *>(m_tabWidget->currentWidget());
}

void MainWindow::exportMarkdownToPdf()
{
    auto *preview = currentMarkdownPreview();
    if (!preview) {
        notify("Open Markdown Preview first (Ctrl+M)", 1);
        return;
    }

    // Suggest filename based on source file
    QString suggested = "preview.pdf";
    auto *srcEditor = preview->property("sourceEditor").value<QWidget *>();
    if (srcEditor) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (m_tabWidget->widget(i) == srcEditor) {
                QString path = m_tabWidget->tabToolTip(i);
                if (!path.isEmpty()) {
                    QFileInfo fi(path);
                    suggested = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
                }
                break;
            }
        }
    }

    QString filePath = QFileDialog::getSaveFileName(this, "Export to PDF", suggested, "PDF Files (*.pdf)");
    if (filePath.isEmpty()) return;

    preview->exportToPdf(filePath, m_settings.pdfMarginLeft, m_settings.pdfMarginRight,
                         m_settings.pdfPageNumbering, m_settings.pdfOrientation == "landscape",
                         m_settings.pdfPageBorder);
    notify("PDF exported: " + QFileInfo(filePath).fileName(), 3);
}

// ── Command Palette ─────────────────────────────────────────────────

void MainWindow::setupCommandPalette()
{
    m_commandPalette->addCommand("Save File", "Ctrl+S", [this]() { saveCurrentFile(); });
    m_commandPalette->addCommand("Settings", "", [this]() { onSettingsTriggered(); });
    m_commandPalette->addCommand("Create Project", "", [this]() { onCreateProject(); });
    m_commandPalette->addCommand("Edit Project", "", [this]() { onEditProject(); });
    m_commandPalette->addCommand("Git Commit", "", [this]() { onCommitClicked(); });
    m_commandPalette->addCommand("SSH Connect", "", [this]() { onSshConnect(); });
    m_commandPalette->addCommand("SSH Disconnect", "", [this]() { onSshDisconnect(); });
    m_commandPalette->addCommand("SSH Tunnels", "", [this]() { onSshTunnels(); });
    m_commandPalette->addCommand("SSH Upload File", "", [this]() { onSshUpload(); });
    m_commandPalette->addCommand("SSH Download File", "", [this]() { onSshDownload(); });
    m_commandPalette->addCommand("Split Editor Horizontal", "", [this]() { splitEditorHorizontal(); });
    m_commandPalette->addCommand("Split Editor Vertical", "", [this]() { splitEditorVertical(); });
    m_commandPalette->addCommand("Unsplit Editor", "", [this]() { unsplitEditor(); });
    m_commandPalette->addCommand("Markdown Preview", "Ctrl+M", [this]() { toggleMarkdownPreview(); });
    m_commandPalette->addCommand("Export Preview to PDF", "", [this]() { exportMarkdownToPdf(); });
    m_commandPalette->addCommand("Show Diff", "", [this]() {
        m_diffViewer->refresh(m_fileBrowser->rootPath());
        m_bottomTabWidget->setCurrentWidget(m_diffViewer);
    });
    m_commandPalette->addCommand("Show Git Graph", "", [this]() {
        m_gitGraph->refresh(m_fileBrowser->rootPath());
        m_bottomTabWidget->setCurrentWidget(m_gitGraph);
    });
    m_commandPalette->addCommand("Show Notifications", "", [this]() {
        m_bottomTabWidget->setCurrentWidget(m_notificationPanel);
    });
    m_commandPalette->addCommand("Show Terminal", "", [this]() {
        m_bottomTabWidget->setCurrentIndex(1);
    });
    m_commandPalette->addCommand("Show Prompt", "", [this]() {
        m_bottomTabWidget->setCurrentIndex(0);
    });
    m_commandPalette->addCommand("Focus AI Terminal", "", [this]() {
        m_tabWidget->setCurrentIndex(0);
    });
    m_commandPalette->addCommand("Show Changes", "", [this]() {
        m_bottomTabWidget->setCurrentWidget(m_changesMonitor);
    });
    m_commandPalette->addCommand("Open File...", "Ctrl+P", [this]() {
        m_fileOpener->setRootPath(m_fileBrowser->rootPath());
        m_fileOpener->show();
    });
    m_commandPalette->addCommand("Search in Workspace", "Ctrl+Shift+F", [this]() {
        m_workspaceSearch->setProjectDir(m_fileBrowser->rootPath());
        m_bottomTabWidget->setCurrentWidget(m_workspaceSearch);
        m_workspaceSearch->focusSearch();
    });
    m_commandPalette->addCommand("Git Blame Current File", "", [this]() {
        blameCurrentFile();
    });

    // Theme switching commands
    for (const auto &et : m_settings.externalThemes) {
        QString theme = et.name;
        m_commandPalette->addCommand("Theme: " + theme, "", [this, theme]() {
            m_settings.globalTheme = theme;
            m_settings.applyThemeDefaults();
            m_settings.save();
            applyGlobalTheme();
            applySettings();
        });
    }
}


// ── Global Theme ────────────────────────────────────────────────────

void MainWindow::applyGlobalTheme()
{
    const ExternalTheme *et = m_settings.findExternalTheme(m_settings.globalTheme);
    if (!et) return;

    bool dark = (et->appearance != "light");
    QString bgColor = et->bgColor, textColor = et->textColor, altBg = et->altBg;
    QString borderColor = et->borderColor, hoverBg = et->hoverBg, selectedBg = et->selectedBg;

    // Compute accent/focus color from selectedBg
    QColor selCol(selectedBg);
    QString focusBorder = selCol.lighter(130).name();

    QString ss = QString(R"(
        /* ── Base ─────────────────────────────────────── */
        QMainWindow { background: %1; }
        QWidget { background: %1; color: %2; }
        QLabel { color: %2; background: transparent; }
        QPlainTextEdit { background-color: %1; color: %2; border: none; }
        QTextEdit { background-color: %1; color: %2; border: none; }

        /* ── Tabs (Zed/VS Code style) ─────────────────── */
        QTabWidget::pane { border: none; background: %1; }
        QTabBar { background: transparent; }
        QTabBar::tab {
            background: %3; color: %2;
            padding: 7px 14px;
            border: none;
            border-bottom: 2px solid transparent;
            margin-right: 1px;
        }
        QTabBar::tab:selected {
            background: %1;
            border-bottom: 2px solid %6;
        }
        QTabBar::tab:hover:!selected { background: %5; }
        QTabBar::close-button { border: none; padding: 4px; border-radius: 4px; }
        QTabBar::close-button:hover { background: rgba(128,128,128,0.35); }

        /* ── Font combo ───────────────────────────────── */
        QFontComboBox { background: %3; color: %2; border: 1px solid %4; padding: 4px 8px; border-radius: 6px; }
        QFontComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %6; border-radius: 6px; }
        QDialogButtonBox QPushButton { min-width: 80px; }

        /* ── Splitter ─────────────────────────────────── */
        QSplitter::handle { background: %4; width: 1px; height: 1px; }

        /* ── Status bar ───────────────────────────────── */
        QStatusBar { background: %3; color: %2; border: none; padding: 2px 4px; }
        QStatusBar QLabel { color: %2; background: transparent; }

        /* ── Inputs ───────────────────────────────────── */
        QLineEdit {
            background: %3; color: %2;
            border: 1px solid %4;
            padding: 5px 10px;
            border-radius: 6px;
        }
        QLineEdit:focus { border-color: )" + focusBorder + R"(; }
        QSpinBox {
            background: %3; color: %2;
            border: 1px solid %4;
            padding: 4px 8px;
            border-radius: 6px;
        }
        QSpinBox:focus { border-color: )" + focusBorder + R"(; }
        QComboBox {
            background: %3; color: %2;
            border: 1px solid %4;
            padding: 5px 10px;
            border-radius: 6px;
        }
        QComboBox:hover { border-color: %5; }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background: %1; color: %2;
            selection-background-color: %6;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 4px;
        }
        QCheckBox { color: %2; spacing: 6px; }
        QCheckBox::indicator {
            width: 16px; height: 16px;
            border: 1px solid %4;
            border-radius: 4px;
            background: %3;
        }
        QCheckBox::indicator:checked {
            background: %6;
            border-color: %6;
        }
        QCheckBox::indicator:hover { border-color: %5; }

        /* ── Buttons (rounded, modern) ────────────────── */
        QPushButton {
            background: %3; color: %2;
            border: 1px solid %4;
            padding: 6px 16px;
            border-radius: 6px;
        }
        QPushButton:hover { background: %5; border-color: %5; }
        QPushButton:pressed { background: %6; }
        QPushButton:disabled { color: %4; background: %1; }
        QToolButton {
            color: %2;
            border: none;
            padding: 4px;
            border-radius: 6px;
        }
        QToolButton:hover { background: %5; }

        /* ── Lists ────────────────────────────────────── */
        QListWidget { background: %1; color: %2; border: none; outline: none; }
        QListWidget::item { padding: 5px 8px; border-radius: 4px; margin: 1px 2px; }
        QListWidget::item:selected { background: %6; }
        QListWidget::item:hover:!selected { background: %5; }

        /* ── Tables ───────────────────────────────────── */
        QTableWidget { background: %1; color: %2; border: none; gridline-color: %4; }
        QTableWidget::item { padding: 4px; }
        QTableWidget::item:selected { background: %6; }
        QHeaderView::section {
            background: %3; color: %2;
            border: none; border-bottom: 1px solid %4;
            padding: 5px 8px;
        }

        /* ── Menus (floating, rounded) ────────────────── */
        QMenu {
            background: %1; color: %2;
            border: 1px solid %4;
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
            margin: 1px 4px;
        }
        QMenu::item:selected { background: %6; }
        QMenu::item:disabled { color: %4; }
        QMenu::separator { height: 1px; background: %4; margin: 4px 8px; }

        /* ── Dialogs ──────────────────────────────────── */
        QDialog { background: %1; color: %2; }
        QGroupBox {
            color: %2;
            border: 1px solid %4;
            border-radius: 8px;
            margin-top: 8px;
            padding: 12px 8px 8px 8px;
        }
        QGroupBox::title { color: %2; subcontrol-origin: margin; left: 12px; padding: 0 6px; }

        /* ── Scroll bars (thin, rounded) ──────────────── */
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical {
            background: %4; border-radius: 4px; min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: %5; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }
        QScrollBar::handle:horizontal {
            background: %4; border-radius: 4px; min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover { background: %5; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

        /* ── Progress bar ─────────────────────────────── */
        QProgressBar {
            background: %3; color: %2;
            border: 1px solid %4;
            border-radius: 6px;
            text-align: center;
        }
        QProgressBar::chunk { background: %6; border-radius: 5px; }

        /* ── Tooltips ─────────────────────────────────── */
        QToolTip {
            background: %3; color: %2;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 4px 8px;
        }

        /* ── Scroll area ──────────────────────────────── */
        QScrollArea { border: none; background: %1; }
    )").arg(bgColor, textColor, altBg, borderColor, hoverBg, selectedBg);

    // Generate tab close icon matching theme text color
    {
        QPixmap pm(16, 16);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(textColor), 1.5));
        p.drawLine(4, 4, 12, 12);
        p.drawLine(12, 4, 4, 12);
        p.end();
        QString iconPath = QDir::tempPath() + "/vibe-coder-tab-close.png";
        pm.save(iconPath);
        ss += QString("QTabBar::close-button { image: url(%1); width: 16px; height: 16px; }").arg(iconPath);
    }

    qApp->setStyleSheet(ss);

    // Title bar specific styling
    m_titleBar->setStyleSheet(
        QString("TitleBar { background: %1; border-bottom: 1px solid %2; }")
            .arg(altBg, borderColor));

    // Set window title bar to match theme (Qt 6.5+)
    auto *hints = QGuiApplication::styleHints();
    hints->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);

    // Force all widgets to repaint with new styles
    for (auto *w : findChildren<QWidget *>())
        w->update();
}

void MainWindow::refreshSavedPrompts()
{
    m_savedPromptsCombo->blockSignals(true);
    m_savedPromptsCombo->clear();
    m_savedPromptsCombo->addItem("-- Saved prompts --");
    if (m_project.isLoaded()) {
        for (const auto &p : m_project.savedPrompts()) {
            QString label = p.left(60).simplified();
            if (p.length() > 60) label += "...";
            m_savedPromptsCombo->addItem(label);
        }
    }
    m_savedPromptsCombo->setCurrentIndex(0);
    m_savedPromptsCombo->blockSignals(false);
}

void MainWindow::saveSession()
{
    QSettings s("vibe-coder", "vibe-coder");

    // Save normal (non-maximized) geometry so we restore to the correct screen
    QRect normalGeom = normalGeometry();
    s.setValue("session/windowPos", normalGeom.topLeft());
    s.setValue("session/windowSize", normalGeom.size());
    s.setValue("session/windowMaximized", isMaximized());
    s.setValue("session/mainSplitter", m_mainSplitter->saveState());
    s.setValue("session/rightSplitter", m_rightSplitter->saveState());
    s.setValue("session/browserPath", m_fileBrowser->rootPath());

    QStringList openFiles;
    QList<QVariant> cursorPositions;
    QList<QVariant> scrollPositions;
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        QString path = m_tabWidget->tabToolTip(i);
        if (!path.isEmpty()) {
            openFiles.append(path);
            auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
            if (editor) {
                cursorPositions.append(editor->textCursor().position());
                scrollPositions.append(editor->verticalScrollBar()->value());
            } else {
                cursorPositions.append(0);
                scrollPositions.append(0);
            }
        }
    }
    s.setValue("session/openFiles", openFiles);
    s.setValue("session/cursorPositions", cursorPositions);
    s.setValue("session/scrollPositions", scrollPositions);
    s.setValue("session/activeTab", m_tabWidget->currentIndex());
    s.setValue("session/bottomTab", m_bottomTabWidget->currentIndex());
}

void MainWindow::restoreSession()
{
    QSettings s("vibe-coder", "vibe-coder");

    if (s.contains("session/windowPos")) {
        QPoint savedPos = s.value("session/windowPos").toPoint();
        QSize savedSize = s.value("session/windowSize", QSize(1200, 800)).toSize();
        bool wasMaximized = s.value("session/windowMaximized", false).toBool();

        // Find the screen that contains the saved position
        QScreen *targetScreen = nullptr;
        for (QScreen *screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().contains(savedPos)) {
                targetScreen = screen;
                break;
            }
        }

        if (targetScreen) {
            if (wasMaximized) {
                // Position on correct screen, then maximize after event loop
                QRect screenGeom = targetScreen->availableGeometry();
                setGeometry(screenGeom);
                QTimer::singleShot(0, this, [this]() {
                    showMaximized();
                });
            } else {
                move(savedPos);
                resize(savedSize);
            }
        } else {
            // Saved monitor not available — keep default position, just restore size
            resize(savedSize);
        }
    }

    if (s.contains("session/mainSplitter"))
        m_mainSplitter->restoreState(s.value("session/mainSplitter").toByteArray());

    if (s.contains("session/rightSplitter"))
        m_rightSplitter->restoreState(s.value("session/rightSplitter").toByteArray());

    if (s.contains("session/browserPath")) {
        QString path = s.value("session/browserPath").toString();
        if (QDir(path).exists())
            m_fileBrowser->setRootPath(path);
    }

    QStringList openFiles = s.value("session/openFiles").toStringList();
    QList<QVariant> cursorPositions = s.value("session/cursorPositions").toList();
    QList<QVariant> scrollPositions = s.value("session/scrollPositions").toList();
    for (int fi = 0; fi < openFiles.size(); ++fi) {
        const QString &path = openFiles[fi];
        if (QFile::exists(path)) {
            onFileOpened(path);
            // Restore cursor and scroll position
            int tabIdx = m_tabWidget->count() - 1;
            auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(tabIdx));
            if (editor) {
                if (fi < cursorPositions.size()) {
                    int pos = cursorPositions[fi].toInt();
                    QTextCursor cursor = editor->textCursor();
                    cursor.setPosition(qMin(pos, editor->document()->characterCount() - 1));
                    editor->setTextCursor(cursor);
                }
                if (fi < scrollPositions.size()) {
                    int scroll = scrollPositions[fi].toInt();
                    editor->verticalScrollBar()->setValue(scroll);
                }
            }
        }
    }

    int activeTab = s.value("session/activeTab", 0).toInt();
    if (activeTab >= 0 && activeTab < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(activeTab);

    int bottomTab = s.value("session/bottomTab", 0).toInt();
    if (bottomTab >= 0 && bottomTab < m_bottomTabWidget->count())
        m_bottomTabWidget->setCurrentIndex(bottomTab);
}

void MainWindow::blameCurrentFile()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 1) return; // no file tab open — silently do nothing

    QString filePath = m_tabWidget->tabToolTip(idx);
    if (filePath.isEmpty()) return;

    // Don't re-blame the same file
    m_gitBlame->blameFile(m_fileBrowser->rootPath(), filePath);
}
