# Diagrams

## Component Layout

```mermaid
graph TB
    subgraph MainWindow
        direction TB
        HM[Hamburger Menu]
        SB[Status Bar: file info | SSH profile combo | progress bar | info]

        subgraph MainSplitter[Main Splitter - Horizontal]
            direction LR
            FB[FileBrowser]

            subgraph RightSplitter[Right Splitter - Vertical]
                direction TB
                subgraph TabWidget[Top Tab Widget]
                    T0[AI-terminal Tab]
                    T1[Editor Tab 1]
                    T2[Editor Tab N]
                end

                subgraph BottomTabWidget[Bottom Tab Widget]
                    PT[Prompt Tab: PromptEdit + Send/Commit/Save]
                    BT[Terminal Tab]
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
    QWidget <|-- LineNumberArea
    QDialog <|-- SettingsDialog
    QDialog <|-- ProjectDialog
    QDialog <|-- SshDialog
    QDialog <|-- SshTunnelDialog
    QObject <|-- SshManager
    QPushButton <|-- ColorButton

    MainWindow --> FileBrowser
    MainWindow --> TerminalWidget : m_terminal + m_bottomTerminal
    MainWindow --> CodeEditor
    MainWindow --> PromptEdit
    MainWindow --> AppSettings
    MainWindow --> Project
    MainWindow --> SshManager

    FileBrowser --> FileItemDelegate
    FileBrowser --> QFileSystemModel : local mode
    FileBrowser --> QStandardItemModel : SSH mode
    CodeEditor --> SyntaxHighlighter
    CodeEditor --> LineNumberArea
    TerminalWidget --> QTermWidget
    SshManager --> SshDialog : saveConnection
    SshTunnelDialog --> SshManager

    class MainWindow {
        -QTabWidget* m_tabWidget
        -QTabWidget* m_bottomTabWidget
        -QSplitter* m_mainSplitter
        -QSplitter* m_rightSplitter
        -SshManager* m_sshManager
        -QComboBox* m_sshProfileCombo
        -QProgressBar* m_transferProgress
        +onFileOpened(QString)
        +saveCurrentFile()
        +onSendClicked()
        +onCommitClicked()
        +onSshConnect()
        +onSshDisconnect()
        +onSshTunnels()
        +onSshUpload()
        +onSshDownload()
    }

    class FileBrowser {
        -QFileSystemModel* m_fsModel
        -QStandardItemModel* m_sshModel
        -QProcess* m_gitProc
        -QString m_sshMountPoint
        +gitStatus(QString) GitStatus
        +setTheme(QString)
        +setSshMount(QString, QString)
        +clearSshMount()
        +toRemotePath(QString) QString
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
        +setShowLineNumbers(bool)
        +setEditorColorScheme(QString)
    }

    class Project {
        -QJsonArray m_prompts
        -QJsonArray m_savedPrompts
        +create(...)
        +addPrompt(QString)
        +addSavedPrompt(int)
        +savedPrompts() QStringList
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
    participant Timer as Health Timer (5s)
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
    participant Timer as Timer (5s)
    participant FileBrowser
    participant GitProc as QProcess

    Timer->>FileBrowser: startGitRefresh()
    Note over FileBrowser: Check m_gitBusy flag

    FileBrowser->>GitProc: git rev-parse --is-inside-work-tree
    GitProc-->>FileBrowser: onGitCheckFinished()

    FileBrowser->>GitProc: git rev-parse --show-toplevel
    GitProc-->>FileBrowser: onGitRootFinished()

    FileBrowser->>GitProc: git status --porcelain -unormal
    GitProc-->>FileBrowser: onGitStatusFinished()

    FileBrowser->>FileBrowser: parseGitOutput()
    FileBrowser->>FileBrowser: rebuildDirCache()
    FileBrowser->>FileBrowser: viewport()->update()
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
    A[SettingsDialog] -->|OK| B[AppSettings struct]
    B -->|save| C[QSettings file]
    B -->|apply| D[Terminal font/scheme]
    B -->|apply| E[Editor font/scheme/lineNumbers]
    B -->|apply| F[FileBrowser font/theme]
    B -->|apply| G[PromptEdit font/colors/sendKey]
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
    PROMPTS --> P1["#1 {id, timestamp, prompt, ...}"]
    PROMPTS --> P2["#2 {id, timestamp, prompt, ...}"]
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
    VAL -->|reject| META[Shell metacharacters: ; & pipe etc.]

    COMMIT[Git Commit] --> GITIGNORE[Auto-add to .gitignore]
    GITIGNORE --> ENV[.env / .env.*]
    GITIGNORE --> PEM[*.pem / *.key]
    GITIGNORE --> CRED[credentials.json]
```
