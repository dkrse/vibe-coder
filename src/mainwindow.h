#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QStatusBar>
#include <QLabel>
#include <QComboBox>
#include <QFileSystemWatcher>
#include <QProgressBar>
#include <QTimer>

#include "terminalwidget.h"
#include "filebrowser.h"
#include "codeeditor.h"
#include "promptedit.h"
#include "settings.h"
#include "project.h"
#include "projectdialog.h"
#include "sshdialog.h"
#include "sshmanager.h"
#include "commandpalette.h"
#include "notificationpanel.h"
#include "diffviewer.h"
#include "changesmonitor.h"
#include "gitgraph.h"
#include "markdownpreview.h"
#include "fileopener.h"
#include "workspacesearch.h"
#include "gitblame.h"

class QSplitter;
class TitleBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFileOpened(const QString &filePath);
    void onSendClicked();
    void onStopClicked();
    void onCommitClicked();
    void onSettingsTriggered();
    void onCreateProject();
    void onEditProject();
    void onSshConnect();
    void onSshDisconnect();
    void onSshTunnels();
    void onSshUpload();
    void onSshDownload();

private:
    void applySettings();
    void applySettingsToEditor(CodeEditor *editor, const QString &lang);

    QTabWidget *m_tabWidget;
    TerminalWidget *m_terminal;
    TerminalWidget *m_bottomTerminal;
    TerminalWidget *m_bottomTerminal2;
    QTabWidget *m_bottomTabWidget;
    FileBrowser *m_fileBrowser;
    PromptEdit *m_editor;
    QPushButton *m_sendBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_savePromptBtn;
    QPushButton *m_repeatPromptBtn;
    QString m_lastPrompt;
    QComboBox *m_savedPromptsCombo;
    void refreshSavedPrompts();
    QToolButton *m_menuBtn;

    AppSettings m_settings;

    QLabel *m_statusFileLabel;
    QLabel *m_statusInfoLabel;

    void updateStatusBar();
    void tryLoadProject(const QString &path);
    void onFileChanged(const QString &path);

    Project m_project;
    QFileSystemWatcher *m_fileWatcher;
    QTimer *m_fileChangeDebounce;
    QSet<QString> m_pendingFileChanges;

    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    void saveCurrentFile();
    void saveSession();
    void restoreSession();
    bool maybeSaveTab(QTabWidget *tabWidget, int index);
    bool hasUnsavedChanges();
    void closeTab(QTabWidget *tabWidget, int index);
    void closeOtherTabs(QTabWidget *tabWidget, int keepIndex);
    void closeAllTabs(QTabWidget *tabWidget);
    void closeTabsToTheRight(QTabWidget *tabWidget, int fromIndex);
    void closeTabsToTheLeft(QTabWidget *tabWidget, int fromIndex);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

    // SSH
    SshManager *m_sshManager;
    QComboBox *m_sshProfileCombo;
    QProgressBar *m_transferProgress;
    QAction *m_sshConnectAction = nullptr;
    QAction *m_sshDisconnectAction = nullptr;
    QAction *m_sshTunnelsAction = nullptr;
    QAction *m_sshUploadAction = nullptr;
    QAction *m_sshDownloadAction = nullptr;
    QString m_localRootBeforeSsh;

    void sshConnectTerminals(const SshConfig &cfg);
    void sshDisconnectTerminals();
    void switchToSshProfile(int index);
    void updateSshProfileCombo();

    // Split view
    void splitEditorHorizontal();
    void splitEditorVertical();
    void unsplitEditor();
    QSplitter *m_editorSplitter;
    QTabWidget *m_splitTabWidget = nullptr;

    // Command palette
    CommandPalette *m_commandPalette;
    void setupCommandPalette();

    // Notifications
    NotificationPanel *m_notificationPanel;
    void notify(const QString &msg, int level = 0); // 0=info,1=warn,2=err,3=ok

    // Diff viewer
    DiffViewer *m_diffViewer;

    // Changes monitor
    ChangesMonitor *m_changesMonitor;

    // Git graph
    GitGraph *m_gitGraph;

    // Markdown previews (multiple allowed, one per source file)
    QList<MarkdownPreview *> m_mdPreviews;
    void toggleMarkdownPreview();
    void exportMarkdownToPdf();
    MarkdownPreview *currentMarkdownPreview();

    // Global theme
    void applyGlobalTheme();

    // Custom title bar
    TitleBar *m_titleBar;

    // Fuzzy file opener (Ctrl+P)
    FileOpener *m_fileOpener;

    // Workspace search (Ctrl+Shift+F)
    WorkspaceSearch *m_workspaceSearch;

    // Git blame
    GitBlame *m_gitBlame;
    void blameCurrentFile();

    // AI-terminal activity indicator
    QLabel *m_aiActivityLabel = nullptr;
    QTimer *m_aiIdleTimer = nullptr;
    QTimer *m_aiAnimTimer = nullptr;
    int m_aiAnimFrame = 0;
};
