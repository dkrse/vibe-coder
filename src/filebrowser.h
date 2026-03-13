#pragma once

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QLineEdit>
#include <QMenu>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QSet>
#include <QColor>
#include <QProcess>
#include <QSortFilterProxyModel>

class FileBrowser;
class FileBrowserProxy;

// Custom roles for SSH model items
enum SshModelRoles {
    FilePathRole = Qt::UserRole + 100,
    IsDirRole    = Qt::UserRole + 101
};

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

    // SSH mount mapping
    void setSshMount(const QString &mountPoint, const QString &remotePrefix);
    void clearSshMount();
    bool isSshActive() const { return !m_sshMountPoint.isEmpty(); }
    QString toRemotePath(const QString &localPath) const;
    QString sshMountPoint() const { return m_sshMountPoint; }
    bool isDark() const { return m_dark; }

    // Unified accessors — work with both models
    QString filePath(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index) const;
    QString fileName(const QModelIndex &index) const;

    // Git status
    enum GitStatus { Clean, Modified, Untracked, Added, Ignored };
    GitStatus gitStatus(const QString &filePath) const;
    bool hasGit() const { return m_hasGit; }

    // Visibility: "visible", "grayed", "hidden"
    void setGitignoreVisibility(const QString &mode);
    void setDotGitVisibility(const QString &mode);
    QString gitignoreVisibility() const { return m_gitignoreVis; }
    QString dotGitVisibility() const { return m_dotGitVis; }

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

    // SSH model helpers
    void sshPopulateDir(QStandardItem *parentItem, const QString &dirPath);
    void onSshItemExpanded(const QModelIndex &index);

    QFileSystemModel *m_fsModel;
    FileBrowserProxy *m_proxyModel;
    QStandardItemModel *m_sshModel;
    QString m_currentRoot;

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
    QSet<QString> m_dirModified;
    QSet<QString> m_dirUntracked;
    QSet<QString> m_dirAdded;
    QSet<QString> m_ignored;
    QSet<QString> m_dirIgnored;

    QProcess *m_gitProc;
    QString m_gitStatusOutput;
    QString m_gitRoot;

    // Visibility
    QString m_gitignoreVis = "grayed";
    QString m_dotGitVis = "hidden";

    // SSH
    QString m_sshMountPoint;
    QString m_sshRemotePrefix;
};

// Proxy model to filter hidden items in file browser
class FileBrowserProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FileBrowserProxy(FileBrowser *fb, QObject *parent = nullptr);
    void refresh();
    void notifyAllChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    FileBrowser *m_fb;
};
