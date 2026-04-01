#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProcess>
#include <QColor>
#include <QMap>

struct GitCommitInfo {
    QString hash;
    QString shortHash;
    QString subject;
    QString author;
    QString date;
    QStringList parents;
    QStringList refs; // branch names, tags
    int column = 0;   // graph column for this commit
    bool isRemoteOnly = false; // true if only referenced by remote branches
};

struct GraphLane {
    QString hash; // the commit hash this lane is tracking
    bool active = false;
};

class GitGraphView : public QWidget {
    Q_OBJECT
public:
    explicit GitGraphView(QWidget *parent = nullptr);
    void setCommits(const QVector<GitCommitInfo> &commits);
    void setColors(const QColor &bg, const QColor &fg);
    void setGraphFont(const QFont &font);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QVector<GitCommitInfo> m_commits;
    QColor m_bgColor = QColor("#1e1e1e");
    QColor m_fgColor = QColor("#d4d4d4");
    QFont m_font;

    static constexpr int ROW_HEIGHT = 32;
    static constexpr int COL_WIDTH = 20;
    static constexpr int LEFT_MARGIN = 10;
    static constexpr int TEXT_OFFSET = 16;
    static constexpr int NODE_RADIUS = 5;

    QColor laneColor(int col) const;
    int m_maxColumns = 1;

    struct GraphRow {
        int column;
        QVector<QPair<int,int>> lines; // from_col -> to_col connections
    };
    QVector<GraphRow> m_graphRows;
    void computeGraph();
};

class SshManager;

class GitGraph : public QWidget {
    Q_OBJECT
public:
    explicit GitGraph(QWidget *parent = nullptr);
    void refresh(const QString &workDir);
    void setViewerFont(const QFont &font);
    void setViewerColors(const QColor &bg, const QColor &fg);
    void setSshInfo(SshManager *mgr, const QString &mountPoint, const QString &remotePrefix);
    void clearSshInfo();

signals:
    void outputMessage(const QString &msg, int level); // 0=info,1=warn,2=err,3=ok
    void commitRequested();

private:
    QScrollArea *m_scrollArea;
    GitGraphView *m_graphView;
    QPushButton *m_refreshBtn;
    QPushButton *m_fetchBtn;
    QPushButton *m_pullBtn;
    QPushButton *m_pushBtn;
    QComboBox *m_branchCombo;
    QLabel *m_trackingLabel;
    QPushButton *m_commitBtn;
    QPushButton *m_userBtn;
    QPushButton *m_remoteBtn;
    QString m_workDir;

    // SSH support
    SshManager *m_sshManager = nullptr;
    QString m_sshMountPoint;
    QString m_sshRemotePrefix;
    bool isSshActive() const { return m_sshManager && !m_sshMountPoint.isEmpty(); }
    QString toRemotePath(const QString &localPath) const;

    // Remotes: name -> url
    QMap<QString, QString> m_remotes;

    void loadBranches();
    void loadLog();
    void loadTrackingInfo();
    void parseTrackingInfo(const QString &raw);
    void loadRemotes();
    void showRemotesDialog();
    void loadUserInfo();
    void showUserDialog();
    void runGitCommand(const QStringList &args, const QString &successMsg, const QString &errorMsg);
    QVector<GitCommitInfo> parseGitLog(const QString &output);
};
