#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QSyntaxHighlighter>

class DiffBlockHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit DiffBlockHighlighter(QTextDocument *parent = nullptr);
protected:
    void highlightBlock(const QString &text) override;
};

struct FileChange {
    QString filePath;
    QDateTime timestamp;
    bool reverted = false;
};

class ChangesMonitor : public QWidget {
    Q_OBJECT
public:
    explicit ChangesMonitor(QWidget *parent = nullptr);

    void setProjectDir(const QString &dir);
    QString projectDir() const { return m_projectDir; }
    void setViewerFont(const QFont &font);
    void setViewerColors(const QColor &bg, const QColor &fg);

    int changeCount() const { return m_changes.size(); }

signals:
    void changeDetected(const QString &filePath);
    void fileReverted(const QString &filePath);
    void openFileRequested(const QString &filePath);

private:
    void onFileChanged(const QString &path);
    void onDirChanged(const QString &path);
    void scanForNewFiles();
    void showDiffForFile(const QString &filePath);
    void revertSelected();
    void refreshList();

    QFileSystemWatcher *m_watcher;
    QTimer *m_scanTimer;
    int m_scanInterval = 10000; // adaptive: 10s → 30s → 60s

    QListWidget *m_fileList;
    QPlainTextEdit *m_diffPreview;
    DiffBlockHighlighter *m_diffHighlighter;
    QPushButton *m_revertBtn;
    QPushButton *m_openBtn;
    QPushButton *m_refreshBtn;
    QPushButton *m_clearBtn;
    QLabel *m_infoLabel;

    QString m_projectDir;
    QVector<FileChange> m_changes;
    QMap<QString, QDateTime> m_knownFiles; // path -> last known mtime

    // Diff cache
    QString m_cachedDiffPath;
    QString m_cachedDiffContent;
};
