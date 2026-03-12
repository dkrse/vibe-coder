#include "mainwindow.h"

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

    // No menu bar - use hamburger button instead
    menuBar()->hide();

    // Central widget
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
        m_tabWidget->removeTab(index);
    });

    // Hamburger menu button in top-right corner of tab widget
    m_menuBtn = new QToolButton;
    m_menuBtn->setText("\u2630"); // ☰ hamburger icon
    m_menuBtn->setAutoRaise(true);
    m_menuBtn->setPopupMode(QToolButton::InstantPopup);
    m_menuBtn->setStyleSheet("QToolButton { font-size: 16px; padding: 4px 8px; }"
                              "QToolButton::menu-indicator { image: none; }");

    auto *hamburgerMenu = new QMenu(m_menuBtn);
    hamburgerMenu->addAction("Create Project...", this, &MainWindow::onCreateProject);
    hamburgerMenu->addAction("Edit Project...", this, &MainWindow::onEditProject);
    hamburgerMenu->addSeparator();
    hamburgerMenu->addAction("Settings...", this, &MainWindow::onSettingsTriggered);
    m_menuBtn->setMenu(hamburgerMenu);

    m_tabWidget->setCornerWidget(m_menuBtn, Qt::TopRightCorner);

    m_terminal = new TerminalWidget;
    m_tabWidget->addTab(m_terminal, "Terminal");
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);

    // Editor + buttons
    auto *editorContainer = new QWidget;
    auto *editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);

    m_editor = new PromptEdit;
    m_editor->setPlaceholderText("Type prompt here...");

    auto *btnLayout = new QHBoxLayout;
    m_sendBtn = new QPushButton("Send");
    m_commitBtn = new QPushButton("Commit");
    btnLayout->addWidget(m_sendBtn);
    btnLayout->addWidget(m_commitBtn);
    btnLayout->addStretch();

    editorLayout->addWidget(m_editor, 1);
    editorLayout->addLayout(btnLayout);

    m_rightSplitter->addWidget(m_tabWidget);
    m_rightSplitter->addWidget(editorContainer);
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

    // Status bar
    m_statusFileLabel = new QLabel("Ready");
    m_statusInfoLabel = new QLabel;
    statusBar()->addWidget(m_statusFileLabel, 1);
    statusBar()->addPermanentWidget(m_statusInfoLabel);

    // File watcher for open tabs
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onFileChanged);

    connect(m_fileBrowser, &FileBrowser::fileOpened, this, &MainWindow::onFileOpened);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_commitBtn, &QPushButton::clicked, this, &MainWindow::onCommitClicked);
    connect(m_editor, &PromptEdit::sendRequested, this, &MainWindow::onSendClicked);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int) {
        updateStatusBar();
    });

    // When file browser directory changes, cd terminal and try load project
    connect(m_fileBrowser, &FileBrowser::rootPathChanged, this, [this](const QString &path) {
        m_terminal->terminal()->changeDir(path);
        tryLoadProject(path);
    });

    // Ctrl+S to save current file
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveCurrentFile);

    applySettings();
    restoreSession();

    // Set initial terminal directory and try loading project
    m_terminal->terminal()->changeDir(m_fileBrowser->rootPath());
    tryLoadProject(m_fileBrowser->rootPath());
}

MainWindow::~MainWindow()
{
    saveSession();
}

void MainWindow::applySettingsToEditor(CodeEditor *editor, const QString &lang)
{
    QFont font(m_settings.editorFontFamily, m_settings.editorFontSize);
    editor->setFont(font);
    editor->setShowLineNumbers(m_settings.showLineNumbers);
    editor->setEditorColorScheme(m_settings.editorColorScheme);

    if (m_settings.syntaxHighlighting && !lang.isEmpty()) {
        editor->highlighter()->setLanguage(lang);
    } else {
        editor->highlighter()->setLanguage("");
    }
}

void MainWindow::updateStatusBar()
{
    int idx = m_tabWidget->currentIndex();
    if (idx == 0) {
        m_statusFileLabel->setText("Terminal");
        m_statusInfoLabel->clear();
    } else {
        QString filePath = m_tabWidget->tabToolTip(idx);
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
    // Terminal
    QFont termFont(m_settings.termFontFamily, m_settings.termFontSize);
    m_terminal->terminal()->setTerminalFont(termFont);
    m_terminal->terminal()->setColorScheme(m_settings.terminalColorScheme);

    // Prompt
    QFont promptFont(m_settings.promptFontFamily, m_settings.promptFontSize);
    m_editor->setFont(promptFont);
    m_editor->setStyleSheet(
        QString("QPlainTextEdit { background-color: %1; color: %2; }")
            .arg(m_settings.promptBgColor.name())
            .arg(m_settings.promptTextColor.name()));
    m_editor->setSendOnEnter(m_settings.promptSendKey == "Enter");

    // File browser
    QFont browserFont(m_settings.browserFontFamily, m_settings.browserFontSize);
    m_fileBrowser->setFont(browserFont);
    m_fileBrowser->setTheme(m_settings.browserTheme);

    // All open file tabs
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
    // Check if already open
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == filePath) {
            m_tabWidget->setCurrentIndex(i);
            return;
        }
    }

    QFileInfo info(filePath);

    // Fix 2: warn on large files (>5MB)
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

    // Watch for external changes
    m_fileWatcher->addPath(filePath);

    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::updateStatusBar);
    updateStatusBar();
}

void MainWindow::saveCurrentFile()
{
    int idx = m_tabWidget->currentIndex();
    if (idx < 1) return; // tab 0 is terminal

    auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(idx));
    if (!editor) return;

    QString filePath = m_tabWidget->tabToolTip(idx);
    if (filePath.isEmpty()) return;

    // Temporarily remove from watcher to avoid reload loop
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
        // Log prompt to project if loaded
        if (m_project.isLoaded()) {
            m_project.addPrompt(text);
        }
        m_terminal->sendText(text);
        m_editor->clear();
        m_tabWidget->setCurrentIndex(0);
    }
}

void MainWindow::onCommitClicked()
{
    QString dir = m_fileBrowser->rootPath();
    QString output;

    QProcess git;
    git.setWorkingDirectory(dir);

    // Init git if not yet initialized
    if (!QDir(dir + "/.git").exists()) {
        git.start("git", {"init"});
        git.waitForFinished(5000);
        output += git.readAllStandardOutput() + git.readAllStandardError();

        // Set up remote if project has one
        if (m_project.isLoaded() && !m_project.gitRemote().isEmpty()) {
            git.start("git", {"remote", "add", "origin", m_project.gitRemote()});
            git.waitForFinished(5000);
            output += git.readAllStandardOutput() + git.readAllStandardError();
        }
    }

    // git add -A (respects .gitignore)
    git.start("git", {"add", "-A"});
    git.waitForFinished(5000);
    output += git.readAllStandardError();

    // Commit with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString message = timestamp;

    git.start("git", {"commit", "-m", message});
    git.waitForFinished(5000);
    output += git.readAllStandardOutput() + git.readAllStandardError();

    m_statusFileLabel->setText(QString("Committed: %1").arg(timestamp));
    if (!output.trimmed().isEmpty()) {
        QMessageBox::information(this, "Commit", output.trimmed());
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

    // Update git remote if changed
    if (!cfg.gitRemote.isEmpty()) {
        QString dir = m_fileBrowser->rootPath();
        if (QDir(dir + "/.git").exists()) {
            QProcess git;
            git.setWorkingDirectory(dir);
            git.start("git", {"remote", "set-url", "origin", cfg.gitRemote});
            git.waitForFinished(3000);
            if (git.exitCode() != 0) {
                // remote might not exist yet
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
    // Reload content of any open tab matching this path
    for (int i = 1; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabToolTip(i) == path) {
            auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
            if (!editor) break;

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                break;

            QTextStream in(&file);
            QString content = in.readAll();

            // Only reload if content actually changed (avoid overwriting user edits)
            if (editor->toPlainText() == content) break;

            // Preserve cursor position
            int cursorPos = editor->textCursor().position();
            editor->setPlainText(content);

            QTextCursor cursor = editor->textCursor();
            cursor.setPosition(qMin(cursorPos, editor->document()->characterCount() - 1));
            editor->setTextCursor(cursor);

            // Re-add to watcher (some systems remove after change)
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
    }
}

void MainWindow::saveSession()
{
    QSettings s("vibe-coder", "vibe-coder");

    s.setValue("session/geometry", saveGeometry());
    s.setValue("session/mainSplitter", m_mainSplitter->saveState());
    s.setValue("session/rightSplitter", m_rightSplitter->saveState());
    s.setValue("session/browserPath", m_fileBrowser->rootPath());

    // Save open file tabs
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

    // Restore open files
    QStringList openFiles = s.value("session/openFiles").toStringList();
    for (const QString &path : openFiles) {
        if (QFile::exists(path))
            onFileOpened(path);
    }

    int activeTab = s.value("session/activeTab", 0).toInt();
    if (activeTab >= 0 && activeTab < m_tabWidget->count())
        m_tabWidget->setCurrentIndex(activeTab);
}
