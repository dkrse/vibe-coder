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
        QString path = m_tabWidget->tabToolTip(index);
        if (!path.isEmpty())
            m_fileWatcher->removePath(path);
        QWidget *w = m_tabWidget->widget(index);
        m_tabWidget->removeTab(index);
        w->deleteLater(); // P0: fix memory leak — removeTab does not delete the widget
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
    m_commitBtn = new QPushButton("Commit");
    m_savePromptBtn = new QPushButton("Save Prompt");
    m_savePromptBtn->setToolTip("Save current prompt for reuse");
    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_commitBtn);
    btnLayout->addWidget(m_savePromptBtn);
    btnLayout->addStretch();

    promptLayout->addWidget(m_editor, 1);
    promptLayout->addLayout(btnLayout);

    m_bottomTabWidget->addTab(promptTab, "Prompt");

    m_bottomTerminal = new TerminalWidget;
    m_bottomTabWidget->addTab(m_bottomTerminal, "Terminal");

    m_rightSplitter->addWidget(m_tabWidget);
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

    // File watcher
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onFileChanged);

    connect(m_fileBrowser, &FileBrowser::fileOpened, this, &MainWindow::onFileOpened);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
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

    // File browser directory changes
    connect(m_fileBrowser, &FileBrowser::rootPathChanged, this, [this](const QString &path) {
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
    });

    connect(m_sshManager, &SshManager::reconnected, this, [this](int idx) {
        m_statusFileLabel->setText(QString("SSH reconnected: %1").arg(m_sshManager->profileLabel(idx)));
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
        if (ok)
            m_statusFileLabel->setText("Transfer complete: " + name);
        else
            QMessageBox::warning(this, "Transfer Error", "Failed: " + name + "\n" + err);
    });

    connect(m_sshManager, &SshManager::sshError, this, [this](const QString &msg) {
        m_statusFileLabel->setText("SSH Error: " + msg);
    });

    // Ctrl+S
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrentFile);

    applySettings();
    restoreSession();

    m_terminal->terminal()->changeDir(m_fileBrowser->rootPath());
    m_bottomTerminal->terminal()->changeDir(m_fileBrowser->rootPath());
    tryLoadProject(m_fileBrowser->rootPath());
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
    QFont termFont(m_settings.termFontFamily, m_settings.termFontSize);
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

    QFont browserFont(m_settings.browserFontFamily, m_settings.browserFontSize);
    m_fileBrowser->setFont(browserFont);
    m_fileBrowser->setTheme(m_settings.browserTheme);

    for (int i = 1; i < m_tabWidget->count(); ++i) {
        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
        if (editor) {
            QString filePath = m_tabWidget->tabToolTip(i);
            QString lang = langFromSuffix(QFileInfo(filePath).suffix());
            applySettingsToEditor(editor, lang);
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
    }
}

void MainWindow::onFileOpened(const QString &filePath)
{
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == filePath) {
            m_tabWidget->setCurrentIndex(i);
            return;
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
        m_statusFileLabel->setText(QString("Saved: %1").arg(filePath));
    } else {
        QMessageBox::warning(this, "Save Error",
            QString("Cannot open '%1' for writing:\n%2").arg(filePath, file.errorString()));
    }

    m_fileWatcher->addPath(filePath);
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
