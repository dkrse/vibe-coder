#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QStatusBar>
#include <QLabel>
#include <QFileSystemWatcher>

#include "terminalwidget.h"
#include "filebrowser.h"
#include "codeeditor.h"
#include "promptedit.h"
#include "settings.h"
#include "project.h"
#include "projectdialog.h"

class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFileOpened(const QString &filePath);
    void onSendClicked();
    void onCommitClicked();
    void onSettingsTriggered();
    void onCreateProject();
    void onEditProject();

private:
    void applySettings();
    void applySettingsToEditor(CodeEditor *editor, const QString &lang);

    QTabWidget *m_tabWidget;
    TerminalWidget *m_terminal;
    FileBrowser *m_fileBrowser;
    PromptEdit *m_editor;
    QPushButton *m_sendBtn;
    QPushButton *m_commitBtn;
    QToolButton *m_menuBtn;

    AppSettings m_settings;

    QLabel *m_statusFileLabel;
    QLabel *m_statusInfoLabel;

    void updateStatusBar();
    void tryLoadProject(const QString &path);
    void onFileChanged(const QString &path);

    Project m_project;
    QFileSystemWatcher *m_fileWatcher;

    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    void saveCurrentFile();
    void saveSession();
    void restoreSession();
};
