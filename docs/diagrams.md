# Diagrams

## Component Layout

```mermaid
graph TB
    subgraph MainWindow
        direction TB
        TB["TitleBar: title / minimize / maximize / close"]
        HM[Hamburger Menu]
        SB["Status Bar: file info | SSH profile combo | progress bar | info"]

        subgraph MainSplitter[Main Splitter - Horizontal]
            direction LR
            FB[FileBrowser]

            subgraph RightSplitter[Right Splitter - Vertical]
                direction TB
                subgraph EditorSplitter[Editor Splitter - Split View]
                    subgraph TabWidget[Top Tab Widget]
                        T0[AI-terminal Tab]
                        T1[Editor Tab 1]
                        T2[Editor Tab N]
                    end
                    STW["Split Tab Widget (optional)"]
                end

                subgraph BottomTabWidget[Bottom Tab Widget]
                    PT[Prompt Tab: PromptEdit + Send/Commit/Save]
                    BT[Terminal Tab]
                    NT[Notifications Tab]
                    DV[Diff Viewer Tab]
                    CM[Changes Tab]
                end
            end
        end
    end
```

## Class Hierarchy

```mermaid
classDiagram
    QMainWindow <|-- MainWindow
    QWidget <|-- FileBrowser
    QWidget <|-- TerminalWidget
    QPlainTextEdit <|-- CodeEditor
    QPlainTextEdit <|-- PromptEdit
    QStyledItemDelegate <|-- FileItemDelegate
    QSyntaxHighlighter <|-- SyntaxHighlighter
    QSyntaxHighlighter <|-- DiffBlockHighlighter
    QWidget <|-- LineNumberArea
    QWidget <|-- FindReplaceBar
    QScrollBar <|-- MarkerScrollBar
    QSortFilterProxyModel <|-- FileBrowserProxy
    QWidget <|-- ChangesMonitor
    QWidget <|-- TitleBar
    QWidget <|-- DialogTitleBar
    QWidget <|-- CommandPalette
    QWidget <|-- NotificationPanel
    QDialog <|-- SettingsDialog
    QDialog <|-- ProjectDialog
    QDialog <|-- SshDialog
    QDialog <|-- SshTunnelDialog
    QObject <|-- SshManager

    MainWindow --> TitleBar
    MainWindow --> FileBrowser
    MainWindow --> TerminalWidget : m_terminal + m_bottomTerminal
    MainWindow --> CodeEditor
    MainWindow --> PromptEdit
    MainWindow --> AppSettings
    MainWindow --> Project
    MainWindow --> SshManager
    MainWindow --> ChangesMonitor
    MainWindow --> CommandPalette
    MainWindow --> NotificationPanel
    MainWindow --> DiffViewer

    FileBrowser --> FileItemDelegate
    FileBrowser --> QFileSystemModel : local mode
    FileBrowser --> QStandardItemModel : SSH mode
    FileBrowser --> QFileSystemWatcher : instant git updates
    CodeEditor --> SyntaxHighlighter
    CodeEditor --> LineNumberArea
    CodeEditor --> FindReplaceBar
    CodeEditor --> MarkerScrollBar
    FileBrowser --> FileBrowserProxy
    TerminalWidget --> QTermWidget
    SshManager --> SshDialog : saveConnection
    SshTunnelDialog --> SshManager
    ChangesMonitor --> DiffBlockHighlighter
    ChangesMonitor --> QFileSystemWatcher
    AppSettings --> ZedThemeLoader : loadZedThemes

    class MainWindow {
        -TitleBar* m_titleBar
        -QTabWidget* m_tabWidget
        -QTabWidget* m_bottomTabWidget
        -QSplitter* m_mainSplitter
        -QSplitter* m_rightSplitter
        -QSplitter* m_editorSplitter
        -SshManager* m_sshManager
        -ChangesMonitor* m_changesMonitor
        -CommandPalette* m_commandPalette
        -NotificationPanel* m_notificationPanel
        -DiffViewer* m_diffViewer
        -QPushButton* m_stopBtn
        -QComboBox* m_sshProfileCombo
        -QProgressBar* m_transferProgress
        +onFileOpened(QString)
        +saveCurrentFile()
        +onSendClicked()
        +onStopClicked()
        +onCommitClicked()
        +maybeSaveTab(QTabWidget*, int)
        +closeTab(QTabWidget*, int)
        +applyGlobalTheme()
        +applySettings()
        +onSshConnect()
        +onSshDisconnect()
        +splitEditorHorizontal()
        +splitEditorVertical()
        +unsplitEditor()
    }

    class FileBrowser {
        -QFileSystemModel* m_fsModel
        -QStandardItemModel* m_sshModel
        -FileBrowserProxy* m_proxyModel
        -QProcess* m_gitProc
        -QFileSystemWatcher* m_fsWatcher
        -QTimer* m_gitDebounce
        -QString m_sshMountPoint
        -QSet~QString~ m_ignored
        +gitStatus(QString) GitStatus
        +setTheme(QString, QColor, QColor)
        +setGitignoreVisibility(QString)
        +setDotGitVisibility(QString)
        +setSshMount(QString, QString)
        +clearSshMount()
        +toRemotePath(QString) QString
    }

    class ChangesMonitor {
        -QFileSystemWatcher* m_watcher
        -QTimer* m_scanTimer
        -QListWidget* m_fileList
        -QPlainTextEdit* m_diffPreview
        -QVector~FileChange~ m_changes
        -QMap~QString,QDateTime~ m_knownFiles
        +setProjectDir(QString)
        +changeCount() int
        +setViewerFont(QFont)
        +setViewerColors(QColor, QColor)
    }

    class SshManager {
        -QList~ProfileState~ m_profiles
        -QMap~int,SshTunnel~ m_tunnels
        -QTimer* m_healthTimer
        -QProcess* m_transferProc
        +addProfile(SshConfig) int
        +connectProfile(int)
        +disconnectProfile(int)
        +uploadFile(int, QString, QString)
        +downloadFile(int, QString, QString)
        +addTunnel(int, SshTunnel) int
        +isValidSshIdentifier(QString)$ bool
    }

    class CodeEditor {
        -LineNumberArea* m_lineNumberArea
        -SyntaxHighlighter* m_highlighter
        -FindReplaceBar* m_findBar
        -MarkerScrollBar* m_markerScrollBar
        -bool m_highlightLine
        +setShowLineNumbers(bool)
        +setEditorColorScheme(QString, QColor, QColor)
        +showFindBar()
        +hideFindBar()
        +setHighlightCurrentLine(bool)
    }

    class AppSettings {
        +QString globalTheme
        +QVector~ZedTheme~ zedThemes
        +QColor bgColor
        +QColor textColor
        +loadZedThemes()
        +applyThemeDefaults()
        +load()
        +save()
    }

    class ZedThemeLoader {
        +loadAll()$ QVector~ZedTheme~
        -parseFile(QString)$ QVector~ZedTheme~
    }
```

## Signal Flow — Prompt Send

```mermaid
sequenceDiagram
    participant User
    participant PromptEdit
    participant MainWindow
    participant Project
    participant Terminal

    User->>PromptEdit: Types prompt + presses send key
    PromptEdit->>MainWindow: sendRequested()
    MainWindow->>Project: addPrompt(text)
    Project->>Project: save() to .LLM/instructions.json
    MainWindow->>Terminal: sendText(text + \r)
    MainWindow->>MainWindow: Switch to terminal tab
```

## Signal Flow — Save and Send

```mermaid
sequenceDiagram
    participant User
    participant PromptEdit
    participant MainWindow
    participant Project

    User->>PromptEdit: Shift+Enter (or Ctrl+Shift+Enter)
    PromptEdit->>MainWindow: saveAndSendRequested()
    MainWindow->>Project: addPrompt(text)
    MainWindow->>Project: addSavedPrompt(lastPromptId)
    MainWindow->>MainWindow: refreshSavedPrompts()
    MainWindow->>MainWindow: sendText + clear + switch tab
```

## Theme Application Flow

```mermaid
sequenceDiagram
    participant User
    participant Settings as Settings/CommandPalette
    participant MW as MainWindow
    participant Global as applyGlobalTheme()
    participant Widgets as applySettings()

    User->>Settings: Select theme
    Settings->>MW: m_settings.globalTheme = theme
    MW->>MW: applyThemeDefaults() (derive colors)
    MW->>Global: Set global QSS stylesheet
    Global->>Global: TitleBar stylesheet
    Global->>Global: Force repaint all children
    MW->>Widgets: Per-widget overrides
    Widgets->>Widgets: Terminal font + colorScheme
    Widgets->>Widgets: Editor palette + line numbers
    Widgets->>Widgets: FileBrowser theme + colors
    Widgets->>Widgets: Prompt stylesheet
```

## SSH Connection Flow (Async)

```mermaid
sequenceDiagram
    participant User
    participant SshDialog
    participant MainWindow
    participant SshManager
    participant sshfs as sshfs process

    User->>SshDialog: Enter credentials
    SshDialog-->>MainWindow: cfg = dlg.result()
    MainWindow->>MainWindow: Validate user/host (isValidSshIdentifier)
    MainWindow->>SshManager: addProfile(cfg)
    MainWindow->>SshManager: connectProfile(idx)
    SshManager->>sshfs: start("sshfs", args) [async]
    Note over MainWindow: UI remains responsive

    alt Success
        sshfs-->>SshManager: finished(exitCode=0)
        SshManager-->>MainWindow: profileConnected(idx)
        MainWindow->>MainWindow: switchToSshProfile(idx)
        MainWindow->>MainWindow: sshConnectTerminals(cfg)
    else Failure
        sshfs-->>SshManager: finished(exitCode!=0)
        SshManager-->>MainWindow: connectFailed(idx, error)
        MainWindow->>SshManager: removeProfile(idx)
    end
```

## SSH Health Check (Async Sequential)

```mermaid
sequenceDiagram
    participant Timer as Health Timer (15s)
    participant SshManager
    participant stat as stat process
    participant sshfs as sshfs process

    Timer->>SshManager: checkNextProfile(0)

    loop For each connected profile
        SshManager->>stat: start("stat", mountPoint) [async]
        Note over SshManager: 3s timeout timer

        alt Healthy
            stat-->>SshManager: exitCode=0
            SshManager->>SshManager: resetReconnectAttempts
            SshManager->>SshManager: checkNextProfile(i+1)
        else Dead/Timeout
            stat-->>SshManager: timeout or exitCode!=0
            SshManager-->>SshManager: connectionLost(i)
            SshManager->>SshManager: fusermount -uz [async]
            SshManager->>sshfs: doMountAsync(i) [reconnect]
            SshManager->>SshManager: checkNextProfile(i+1)
        end
    end
```

## Async Git Commit Flow

```mermaid
sequenceDiagram
    participant User
    participant MainWindow
    participant GitProc as QProcess (chained)

    User->>MainWindow: Click Commit
    MainWindow->>MainWindow: Update .gitignore (sensitive files)
    MainWindow->>MainWindow: Disable commit button

    alt No .git directory
        MainWindow->>GitProc: git init [async]
        GitProc-->>MainWindow: finished
        MainWindow->>GitProc: git remote add origin [async]
        GitProc-->>MainWindow: finished
    end

    MainWindow->>GitProc: git add -A [async]
    GitProc-->>MainWindow: finished
    MainWindow->>GitProc: git commit -m timestamp [async]
    GitProc-->>MainWindow: finished
    MainWindow->>MainWindow: Enable commit button + status update
```

## Git Status Pipeline (Async)

```mermaid
sequenceDiagram
    participant FSW as QFileSystemWatcher
    participant Timer as Timer (10s)
    participant FileBrowser
    participant GitProc as QProcess

    alt Instant trigger
        FSW-->>FileBrowser: fileChanged / directoryChanged
        FileBrowser->>FileBrowser: 300ms debounce
    else Periodic poll
        Timer->>FileBrowser: startGitRefresh()
    end

    Note over FileBrowser: Check m_gitBusy flag

    FileBrowser->>GitProc: git rev-parse --is-inside-work-tree
    GitProc-->>FileBrowser: onGitCheckFinished()

    FileBrowser->>GitProc: git rev-parse --show-toplevel
    GitProc-->>FileBrowser: onGitRootFinished()

    FileBrowser->>GitProc: git status --porcelain -unormal
    GitProc-->>FileBrowser: onGitStatusFinished()

    FileBrowser->>GitProc: git ls-files --others --ignored
    GitProc-->>FileBrowser: onGitLsIgnoredFinished()

    FileBrowser->>GitProc: git check-ignore --no-index --stdin
    GitProc-->>FileBrowser: onGitCheckIgnoreFinished()

    FileBrowser->>FileBrowser: parseGitOutput()
    FileBrowser->>FileBrowser: rebuildDirCache()
    FileBrowser->>FileBrowser: viewport()->update()
```

## Changes Monitor Flow

```mermaid
sequenceDiagram
    participant FS as QFileSystemWatcher
    participant Timer as Scan Timer (10s)
    participant CM as ChangesMonitor
    participant MainWindow
    participant Git as git process

    alt File modified
        FS-->>CM: fileChanged(path)
        CM->>CM: Check mtime, deduplicate
        CM->>CM: Prepend to m_changes
        CM-->>MainWindow: changeDetected(path)
        MainWindow->>MainWindow: Update tab badge "Changes (N)"
    else New file detected
        Timer->>CM: scanForNewFiles()
        CM->>CM: Compare against m_knownFiles
        CM-->>MainWindow: changeDetected(path)
    end

    CM->>Git: git diff -- relPath [async]
    Git-->>CM: diff output → m_diffPreview

    alt User clicks Revert
        CM->>Git: git checkout -- relPath [async]
        Git-->>CM: success
        CM-->>MainWindow: fileReverted(path)
    end
```

## File Transfer via sshfs Mount

```mermaid
sequenceDiagram
    participant User
    participant MainWindow
    participant SshManager
    participant rsync as rsync/cp process

    User->>MainWindow: SSH Upload File
    MainWindow->>MainWindow: QFileDialog (select local file)
    MainWindow->>SshManager: uploadFile(idx, local, remote)
    Note over SshManager: destPath = mountPoint + remotePath
    SshManager->>rsync: start("rsync", ["--progress", src, dest])

    loop Progress updates
        rsync-->>SshManager: readyReadStandardOutput
        SshManager-->>MainWindow: transferProgress(name, pct%)
        MainWindow->>MainWindow: Update progress bar
    end

    rsync-->>SshManager: finished
    SshManager-->>MainWindow: transferFinished(name, ok, err)
    MainWindow->>MainWindow: Hide progress bar
```

## File Open Flow

```mermaid
sequenceDiagram
    participant User
    participant FileBrowser
    participant MainWindow
    participant CodeEditor
    participant FileWatcher

    User->>FileBrowser: Click on file
    FileBrowser->>MainWindow: fileOpened(path)
    MainWindow->>MainWindow: Check duplicate tabs
    MainWindow->>MainWindow: Check file size (>5MB warning)
    MainWindow->>CodeEditor: new CodeEditor()
    MainWindow->>CodeEditor: setPlainText(content)
    MainWindow->>CodeEditor: setLanguage() then setEditorColorScheme()
    Note over CodeEditor: Single rehighlight pass
    MainWindow->>FileWatcher: addPath(filePath)
```

## Settings Data Flow

```mermaid
flowchart LR
    A[SettingsDialog - Tabbed] -->|OK| B[AppSettings struct]
    B -->|applyThemeDefaults| TH[Global Theme Cascade]
    ZED[Zed Theme JSON] -->|ZedThemeLoader| B
    TH --> D & E & F & G & DV & CM2
    B -->|save| C[QSettings file]
    B -->|applyGlobalTheme| SS[Global QSS Stylesheet]
    SS --> ALL[All Widgets + TitleBar]
    B -->|applySettings| D[Terminal font/scheme]
    B -->|applySettings| E[Editor font/scheme/lineNumbers/highlightLine]
    B -->|applySettings| VIS[FileBrowser visibility filtering]
    B -->|applySettings| F[FileBrowser font/theme/colors]
    B -->|applySettings| G[PromptEdit font/colors/sendKey/highlightLine]
    B -->|applySettings| DV[DiffViewer font]
    B -->|applySettings| CM2[ChangesMonitor font]
    C -->|load on startup| B
```

## Project Structure on Disk

```mermaid
graph TD
    Root[Project Directory] --> LLM[.LLM/]
    Root --> GI[.gitignore]
    Root --> SRC[Source files...]
    LLM --> IJ[instructions.json]

    IJ --> PN[project_name]
    IJ --> PH[project_hash]
    IJ --> DESC[description]
    IJ --> MODEL[model]
    IJ --> GR[git_remote]
    IJ --> PROMPTS[prompts array]
    IJ --> SP[saved_prompts array - IDs]
    PROMPTS --> P1["#1 id, timestamp, prompt, ..."]
    PROMPTS --> P2["#2 id, timestamp, prompt, ..."]
    PROMPTS --> PN2["#N ..."]
```

## Security Model

```mermaid
flowchart TD
    PWD[SSH Password] --> T1[Terminal: sent via PTY - echo off]
    PWD --> T2[sshfs: password_stdin option]
    PWD --> T3[Tunnels: SSH_ASKPASS temp script]
    PWD --> T4[Transfers: not needed - uses sshfs mount]
    PWD -.->|NEVER| PS[Process args / ps aux]
    PWD -.->|NEVER| DISK[QSettings / disk]

    INPUT[User/Host input] --> VAL[isValidSshIdentifier]
    VAL -->|reject| META["Shell metacharacters: ; & pipe etc."]

    COMMIT[Git Commit] --> GITIGNORE[Auto-add to .gitignore]
    GITIGNORE --> ENV[.env / .env.*]
    GITIGNORE --> PEM[*.pem / *.key]
    GITIGNORE --> CRED[credentials.json]
```
