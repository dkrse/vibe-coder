#include "mainwindow.h"
#include "sshtunneldialog.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QFont>
#include <QProcess>
#include <QToolBar>
#include <QMenuBar>
#include <QDateTime>
#include <QShortcut>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QTimer>
#include <QCloseEvent>

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
    return "";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Vibe Coder");
    resize(1200, 800);

    m_settings.load();
    m_sshManager = new SshManager(this);

    menuBar()->hide();

    auto *central = new QWidget(this);
    setCentralWidget(central);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    m_rightSplitter = new QSplitter(Qt::Vertical);

    // Tab widget with hamburger button in corner
    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (index == 0) return;
        if (!maybeSaveTab(m_tabWidget, index)) return;
        QString path = m_tabWidget->tabToolTip(index);
        if (!path.isEmpty())
            m_fileWatcher->removePath(path);
        QWidget *w = m_tabWidget->widget(index);
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
    m_commitBtn = new QPushButton("Commit");
    m_savePromptBtn = new QPushButton("Save Prompt");
    m_savePromptBtn->setToolTip("Save current prompt for reuse");
    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addWidget(m_commitBtn);
    btnLayout->addWidget(m_savePromptBtn);
    btnLayout->addStretch();

    promptLayout->addWidget(m_editor, 1);
    promptLayout->addLayout(btnLayout);

    m_bottomTabWidget->addTab(promptTab, "Prompt");

    m_bottomTerminal = new TerminalWidget;
    m_bottomTabWidget->addTab(m_bottomTerminal, "Terminal");

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
        } else if (m_bottomTabWidget->widget(idx) == m_changesMonitor) {
            // Clear badge
            m_bottomTabWidget->setTabText(idx, "Changes");
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

    m_mainSplitter->addWidget(m_fileBrowser);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_mainSplitter);

    // Status bar: file label | SSH profile combo | transfer progress | info label
    m_statusFileLabel = new QLabel("Ready");
    m_statusInfoLabel = new QLabel;
    m_sshProfileCombo = new QComboBox;
    m_sshProfileCombo->setFixedWidth(180);
    m_sshProfileCombo->addItem("No SSH");
    m_sshProfileCombo->hide();
    m_transferProgress = new QProgressBar;
    m_transferProgress->setFixedWidth(150);
    m_transferProgress->setTextVisible(true);
    m_transferProgress->hide();

    statusBar()->addWidget(m_statusFileLabel, 1);
    statusBar()->addPermanentWidget(m_sshProfileCombo);
    statusBar()->addPermanentWidget(m_transferProgress);
    statusBar()->addPermanentWidget(m_statusInfoLabel);

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
    connect(m_commitBtn, &QPushButton::clicked, this, &MainWindow::onCommitClicked);
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
        m_tabWidget->setCurrentIndex(0);
    });

    // Saved prompts
    connect(m_savePromptBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_editor->toPlainText().trimmed();
        if (text.isEmpty()) return;
        if (!m_project.isLoaded()) {
            QMessageBox::information(this, "Save Prompt", "No project loaded. Create a project first.");
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

    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int) {
        updateStatusBar();
    });

    // File browser directory changes — also update changes monitor
    connect(m_fileBrowser, &FileBrowser::rootPathChanged, this, [this](const QString &path) {
        m_changesMonitor->setProjectDir(path);
        if (m_sshManager->activeProfileIndex() >= 0) {
            QString remotePath = m_fileBrowser->toRemotePath(path);
            m_terminal->sendText("cd " + remotePath);
            m_bottomTerminal->sendText("cd " + remotePath);
        } else {
            m_terminal->terminal()->changeDir(path);
            m_bottomTerminal->terminal()->changeDir(path);
        }
        tryLoadProject(path);
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

        // Complete async connection: setup file browser + terminals
        switchToSshProfile(idx);
        sshConnectTerminals(m_sshManager->profileConfig(idx));
    });

    connect(m_sshManager, &SshManager::connectFailed, this, [this](int idx, const QString &err) {
        m_sshManager->removeProfile(idx);
        m_statusFileLabel->setText("SSH connect failed");
        notify("SSH connect failed: " + err, 2);
        QMessageBox::warning(this, "SSH Error", err);
    });

    connect(m_sshManager, &SshManager::profileDisconnected, this, [this](int) {
        updateSshProfileCombo();
        if (m_sshManager->activeProfileIndex() < 0) {
            m_sshDisconnectAction->setEnabled(false);
            m_sshTunnelsAction->setEnabled(false);
            m_sshUploadAction->setEnabled(false);
            m_sshDownloadAction->setEnabled(false);
            m_sshProfileCombo->hide();
            m_fileBrowser->clearSshMount();
            if (!m_localRootBeforeSsh.isEmpty())
                m_fileBrowser->setRootPath(m_localRootBeforeSsh);
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
            QMessageBox::warning(this, "Transfer Error", "Failed: " + name + "\n" + err);
        }
    });

    connect(m_sshManager, &SshManager::sshError, this, [this](const QString &msg) {
        m_statusFileLabel->setText("SSH Error: " + msg);
    });

    // Ctrl+S
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrentFile);

    // Command palette
    m_commandPalette = new CommandPalette(this);
    setupCommandPalette();
    auto *paletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(paletteShortcut, &QShortcut::activated, m_commandPalette, &CommandPalette::show);

    applySettings();
    applyGlobalTheme();
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

    m_terminal->terminal()->changeDir(m_fileBrowser->rootPath());
    m_bottomTerminal->terminal()->changeDir(m_fileBrowser->rootPath());
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
    if (dlg.exec() != QDialog::Accepted)
        return;

    SshConfig cfg = dlg.result();
    if (cfg.host.isEmpty() || cfg.user.isEmpty()) {
        QMessageBox::warning(this, "SSH", "User and host are required.");
        return;
    }

    // Validate against command injection
    if (!SshManager::isValidSshIdentifier(cfg.user) || !SshManager::isValidSshIdentifier(cfg.host)) {
        QMessageBox::warning(this, "SSH", "Invalid characters in user or host.");
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

    sshDisconnectTerminals();
    m_sshManager->disconnectProfile(idx);
}

void MainWindow::switchToSshProfile(int index)
{
    if (!m_sshManager->isConnected(index)) return;
    m_sshManager->setActiveProfile(index);

    const auto &cfg = m_sshManager->profileConfig(index);
    QString mp = m_sshManager->mountPoint(index);

    m_fileBrowser->setSshMount(mp, "");
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
    if (!cfg.identityFile.isEmpty())
        sshLine += " -i " + cfg.identityFile;

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
            m_terminal->sendText("cd " + remotePath);
            m_bottomTerminal->sendText("cd " + remotePath);
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
        QMessageBox::information(this, "SSH Tunnels", "No active SSH connection.");
        return;
    }
    SshTunnelDialog dlg(m_sshManager, idx, this);
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
    editor->setFont(font);
    editor->setShowLineNumbers(m_settings.showLineNumbers);

    // Set language first (no rehighlight), then color scheme triggers single rehighlight
    if (m_settings.syntaxHighlighting && !lang.isEmpty()) {
        editor->highlighter()->setLanguage(lang);
    } else {
        editor->highlighter()->setLanguage("");
    }
    editor->setEditorColorScheme(m_settings.editorColorScheme);
    editor->setHighlightCurrentLine(m_settings.editorHighlightLine);
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
    QFont termFont;
    termFont.setFamily(m_settings.termFontFamily);
    termFont.setPointSize(m_settings.termFontSize);
    termFont.setStyleHint(QFont::Monospace);
    termFont.setFixedPitch(true);
    m_terminal->terminal()->setTerminalFont(termFont);
    m_terminal->terminal()->setColorScheme(m_settings.terminalColorScheme);
    m_bottomTerminal->terminal()->setTerminalFont(termFont);
    m_bottomTerminal->terminal()->setColorScheme(m_settings.terminalColorScheme);

    QFont promptFont(m_settings.promptFontFamily, m_settings.promptFontSize);
    m_editor->setFont(promptFont);
    m_editor->setStyleSheet(
        QString("QPlainTextEdit { background-color: %1; color: %2; }")
            .arg(m_settings.promptBgColor.name())
            .arg(m_settings.promptTextColor.name()));
    m_editor->setSendOnEnter(m_settings.promptSendKey == "Enter");
    m_editor->setHighlightCurrentLine(m_settings.promptHighlightLine);

    QFont browserFont(m_settings.browserFontFamily, m_settings.browserFontSize);
    m_fileBrowser->setFont(browserFont);
    m_fileBrowser->setTheme(m_settings.browserTheme);
    m_fileBrowser->setGitignoreVisibility(m_settings.gitignoreVisibility);
    m_fileBrowser->setDotGitVisibility(m_settings.dotGitVisibility);

    // Diff viewer
    QFont diffFont(m_settings.diffFontFamily, m_settings.diffFontSize);
    m_diffViewer->setViewerFont(diffFont);
    m_diffViewer->setViewerColors(m_settings.diffBgColor, m_settings.diffTextColor);

    // Changes monitor
    QFont changesFont(m_settings.changesFontFamily, m_settings.changesFontSize);
    m_changesMonitor->setViewerFont(changesFont);
    m_changesMonitor->setViewerColors(m_settings.changesBgColor, m_settings.changesTextColor);

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
    if (dlg.exec() == QDialog::Accepted) {
        m_settings = dlg.result();
        m_settings.save();
        applySettings();
        applyGlobalTheme();
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

    static const qint64 MAX_FILE_SIZE = 5 * 1024 * 1024;
    if (info.size() > MAX_FILE_SIZE) {
        auto reply = QMessageBox::question(this, "Large File",
            QString("'%1' is %2 MB. Opening large files may be slow.\n\nOpen anyway?")
                .arg(info.fileName())
                .arg(info.size() / (1024.0 * 1024.0), 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString content = in.readAll();

    QString lang = langFromSuffix(info.suffix());

    auto *editor = new CodeEditor;
    editor->setPlainText(content);
    applySettingsToEditor(editor, lang);

    int idx = m_tabWidget->addTab(editor, info.fileName());
    m_tabWidget->setTabToolTip(idx, filePath);
    m_tabWidget->setCurrentIndex(idx);

    m_fileWatcher->addPath(filePath);

    QString fileDir = info.absolutePath();
    if (m_sshManager->activeProfileIndex() >= 0) {
        QString remoteDir = m_fileBrowser->toRemotePath(fileDir);
        m_terminal->sendText("cd " + remoteDir);
        m_bottomTerminal->sendText("cd " + remoteDir);
    } else {
        m_terminal->terminal()->changeDir(fileDir);
        m_bottomTerminal->terminal()->changeDir(fileDir);
    }

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
            QMessageBox::warning(this, "Save Error",
                QString("Failed to write to '%1'.").arg(filePath));
        }
        file.close();
        editor->setProperty("savedContent", editor->toPlainText());
        editor->document()->setModified(false);
        m_tabWidget->setTabText(idx, QFileInfo(filePath).fileName());
        m_statusFileLabel->setText(QString("Saved: %1").arg(filePath));
    } else {
        QMessageBox::warning(this, "Save Error",
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

    auto reply = QMessageBox::question(this, "Unsaved Changes",
        QString("'%1' has unsaved changes.\n\nDo you want to save before closing?").arg(fileName),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (reply == QMessageBox::Cancel)
        return false;

    if (reply == QMessageBox::Save) {
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
            QMessageBox::warning(this, "Save Error",
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
        auto reply = QMessageBox::question(this, "Unsaved Changes",
            "You have unsaved changes. What do you want to do?",
            QMessageBox::SaveAll | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::SaveAll);

        if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }

        if (reply == QMessageBox::SaveAll) {
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

void MainWindow::closeTab(QTabWidget *tabWidget, int index)
{
    if (index == 0) return; // never close AI-terminal
    if (!maybeSaveTab(tabWidget, index)) return;
    QString path = tabWidget->tabToolTip(index);
    if (!path.isEmpty())
        m_fileWatcher->removePath(path);
    QWidget *w = tabWidget->widget(index);
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
        if (m_project.isLoaded())
            m_project.addPrompt(text);
        m_terminal->sendText(text);
        m_editor->clear();
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
    m_commitBtn->setEnabled(false);
    m_statusFileLabel->setText("Committing...");

    // Run git operations async in a chain
    auto *git = new QProcess(this);
    git->setWorkingDirectory(dir);
    auto *output = new QString;

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
    auto doAdd = [this, git, output, dir]() {
        connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, git, output, dir](int, QProcess::ExitStatus) {
            *output += git->readAllStandardOutput() + git->readAllStandardError();
            // Step 3: commit
            disconnect(git, nullptr, this, nullptr);
            connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, git, output](int, QProcess::ExitStatus) {
                *output += git->readAllStandardOutput() + git->readAllStandardError();
                QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
                m_statusFileLabel->setText(QString("Committed: %1").arg(timestamp));
                notify("Git commit: " + timestamp, 3);
                m_commitBtn->setEnabled(true);
                if (!output->trimmed().isEmpty())
                    m_statusFileLabel->setText(output->trimmed().left(100));
                git->deleteLater();
                delete output;
            });
            QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            git->start("git", {"commit", "-m", ts});
        });
        git->start("git", {"add", "-A"});
    };

    if (!QDir(dir + "/.git").exists()) {
        connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, git, output, dir, doAdd](int, QProcess::ExitStatus) {
            *output += git->readAllStandardOutput() + git->readAllStandardError();
            disconnect(git, nullptr, this, nullptr);

            if (m_project.isLoaded() && !m_project.gitRemote().isEmpty()) {
                connect(git, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [git, output, doAdd](int, QProcess::ExitStatus) {
                    *output += git->readAllStandardOutput() + git->readAllStandardError();
                    disconnect(git, nullptr, nullptr, nullptr);
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
        QMessageBox::information(this, "Project",
            "Project already exists. Use 'Edit Project' to modify.");
        tryLoadProject(dir);
        return;
    }

    ProjectConfig cfg;
    cfg.name = QFileInfo(dir).fileName();
    cfg.model = "claude-opus-4-6";

    ProjectDialog dlg("Create Project", cfg, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    cfg = dlg.result();
    if (cfg.name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Project name is required.");
        return;
    }

    if (m_project.create(dir, cfg.name, cfg.description, cfg.model, cfg.gitRemote)) {
        m_statusFileLabel->setText(
            QString("Project: %1 [%2]").arg(cfg.name, cfg.model));
    } else {
        QMessageBox::warning(this, "Error", "Failed to create project.");
    }
}

void MainWindow::onEditProject()
{
    if (!m_project.isLoaded()) {
        QMessageBox::warning(this, "No Project",
            "No project loaded. Create a project first.");
        return;
    }

    ProjectConfig cfg;
    cfg.name = m_project.projectName();
    cfg.description = m_project.description();
    cfg.model = m_project.model();
    cfg.gitRemote = m_project.gitRemote();

    ProjectDialog dlg("Edit Project", cfg, this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    cfg = dlg.result();
    m_project.update(cfg.name, cfg.description, cfg.model, cfg.gitRemote);

    if (!cfg.gitRemote.isEmpty()) {
        QString dir = m_fileBrowser->rootPath();
        if (QDir(dir + "/.git").exists()) {
            QProcess git;
            git.setWorkingDirectory(dir);
            git.start("git", {"remote", "set-url", "origin", cfg.gitRemote});
            git.waitForFinished(3000);
            if (git.exitCode() != 0) {
                git.start("git", {"remote", "add", "origin", cfg.gitRemote});
                git.waitForFinished(3000);
            }
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
            editor->setPlainText(content);

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
    m_commandPalette->addCommand("Show Diff", "", [this]() {
        m_diffViewer->refresh(m_fileBrowser->rootPath());
        m_bottomTabWidget->setCurrentWidget(m_diffViewer);
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

    // Theme switching commands
    for (const QString &theme : {"Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light", "Nord"}) {
        m_commandPalette->addCommand("Theme: " + theme, "", [this, theme]() {
            m_settings.globalTheme = theme;
            m_settings.applyThemeDefaults();
            m_settings.save();
            applySettings();
            applyGlobalTheme();
        });
    }
}


// ── Global Theme ────────────────────────────────────────────────────

void MainWindow::applyGlobalTheme()
{
    bool dark = (m_settings.globalTheme != "Light" && m_settings.globalTheme != "Solarized Light");

    QString bgColor, textColor, altBg, borderColor, hoverBg, selectedBg;

    if (m_settings.globalTheme == "Dark") {
        bgColor = "#1e1e1e"; textColor = "#d4d4d4"; altBg = "#252526";
        borderColor = "#3c3c3c"; hoverBg = "#2a2d2e"; selectedBg = "#094771";
    } else if (m_settings.globalTheme == "Light") {
        bgColor = "#ffffff"; textColor = "#333333"; altBg = "#f3f3f3";
        borderColor = "#e0e0e0"; hoverBg = "#e8e8e8"; selectedBg = "#0060c0";
    } else if (m_settings.globalTheme == "Monokai") {
        bgColor = "#272822"; textColor = "#f8f8f2"; altBg = "#2e2f2a";
        borderColor = "#49483e"; hoverBg = "#3e3d32"; selectedBg = "#49483e";
    } else if (m_settings.globalTheme == "Solarized Dark") {
        bgColor = "#002b36"; textColor = "#839496"; altBg = "#073642";
        borderColor = "#586e75"; hoverBg = "#073642"; selectedBg = "#2aa198";
    } else if (m_settings.globalTheme == "Solarized Light") {
        bgColor = "#fdf6e3"; textColor = "#657b83"; altBg = "#eee8d5";
        borderColor = "#93a1a1"; hoverBg = "#eee8d5"; selectedBg = "#2aa198";
    } else if (m_settings.globalTheme == "Nord") {
        bgColor = "#2e3440"; textColor = "#d8dee9"; altBg = "#3b4252";
        borderColor = "#4c566a"; hoverBg = "#434c5e"; selectedBg = "#5e81ac";
    } else {
        return;
    }

    QString ss = QString(
        "QMainWindow { background: %1; }"
        "QTabWidget::pane { border: 1px solid %4; background: %1; }"
        "QTabBar::tab { background: %3; color: %2; padding: 6px 12px; border: 1px solid %4; }"
        "QTabBar::tab:selected { background: %1; }"
        "QTabBar::tab:hover { background: %5; }"
        "QSplitter::handle { background: %4; }"
        "QStatusBar { background: %3; color: %2; border-top: 1px solid %4; }"
        "QStatusBar QLabel { color: %2; }"
        "QComboBox { background: %3; color: %2; border: 1px solid %4; padding: 3px; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; selection-background-color: %6; }"
        "QPushButton { background: %3; color: %2; border: 1px solid %4; padding: 4px 12px; border-radius: 3px; }"
        "QPushButton:hover { background: %5; }"
        "QToolButton { color: %2; }"
        "QMenu { background: %1; color: %2; border: 1px solid %4; }"
        "QMenu::item:selected { background: %6; }"
        "QMenu::item:disabled { color: %4; }"
        "QProgressBar { background: %3; color: %2; border: 1px solid %4; }"
        "QProgressBar::chunk { background: %6; }"
    ).arg(bgColor, textColor, altBg, borderColor, hoverBg, selectedBg);

    setStyleSheet(ss);
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

    s.setValue("session/geometry", saveGeometry());
    s.setValue("session/mainSplitter", m_mainSplitter->saveState());
    s.setValue("session/rightSplitter", m_rightSplitter->saveState());
    s.setValue("session/browserPath", m_fileBrowser->rootPath());

    QStringList openFiles;
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        QString path = m_tabWidget->tabToolTip(i);
        if (!path.isEmpty())
            openFiles.append(path);
    }
    s.setValue("session/openFiles", openFiles);
    s.setValue("session/activeTab", m_tabWidget->currentIndex());
}

void MainWindow::restoreSession()
{
    QSettings s("vibe-coder", "vibe-coder");

    if (s.contains("session/geometry"))
        restoreGeometry(s.value("session/geometry").toByteArray());

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
    for (const QString &path : openFiles) {
        if (QFile::exists(path))
            onFileOpened(path);
    }

    int activeTab = s.value("session/activeTab", 0).toInt();
    if (activeTab >= 0 && activeTab < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(activeTab);
}
