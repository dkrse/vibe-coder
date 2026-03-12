#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QSet>
#include <QColor>
#include <QProcess>

class FileBrowser;

// Zed-style delegate with git status colors
class FileItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setFileBrowser(FileBrowser *fb) { m_fb = fb; }

private:
    FileBrowser *m_fb = nullptr;
};

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const;
    void setFont(const QFont &font);
    void setTheme(const QString &theme); // "Dark" or "Light"
    bool isDark() const { return m_dark; }

    // Git status
    enum GitStatus { Clean, Modified, Untracked, Added };
    GitStatus gitStatus(const QString &filePath) const;
    bool hasGit() const { return m_hasGit; }

signals:
    void fileOpened(const QString &filePath);
    void rootPathChanged(const QString &path);

private:
    void showContextMenu(const QPoint &pos);
    QString selectedDirPath() const;
    void applyStyle();
    void startGitRefresh();
    void onGitCheckFinished(int exitCode, QProcess::ExitStatus status);
    void onGitStatusFinished(int exitCode, QProcess::ExitStatus status);
    void onGitRootFinished(int exitCode, QProcess::ExitStatus status);
    void parseGitOutput();
    void rebuildDirCache();

    QFileSystemModel *m_model;
    QTreeView *m_treeView;
    QLineEdit *m_pathEdit;
    QPushButton *m_openBtn;
    FileItemDelegate *m_delegate;
    QTimer *m_gitTimer;

    bool m_dark = true;
    QColor m_bgColor;
    QColor m_textColor;

    bool m_hasGit = false;
    bool m_gitBusy = false;
    QSet<QString> m_modified;
    QSet<QString> m_untracked;
    QSet<QString> m_added;
    // Cache: directories that contain changed files
    QSet<QString> m_dirModified;
    QSet<QString> m_dirUntracked;
    QSet<QString> m_dirAdded;

    QProcess *m_gitProc;
    QString m_gitStatusOutput;
    QString m_gitRoot;
};
