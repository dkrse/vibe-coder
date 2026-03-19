#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProcess>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QSplitter>

struct SearchResult {
    QString filePath;
    int line;
    QString lineText;
};

class WorkspaceSearch : public QWidget {
    Q_OBJECT
public:
    explicit WorkspaceSearch(QWidget *parent = nullptr);

    void setProjectDir(const QString &dir);
    void focusSearch();
    void setViewerFont(const QFont &font);
    void setViewerColors(const QColor &bg, const QColor &fg);

signals:
    void fileRequested(const QString &filePath, int line);

private:
    void doSearch();
    void onSearchFinished(int exitCode, QProcess::ExitStatus status);
    void onResultClicked(QListWidgetItem *item);

    QLineEdit *m_searchEdit;
    QCheckBox *m_caseCheck;
    QCheckBox *m_regexCheck;
    QPushButton *m_searchBtn;
    QLabel *m_statusLabel;
    QListWidget *m_resultList;
    QPlainTextEdit *m_preview;
    QSplitter *m_splitter;

    QString m_projectDir;
    QProcess *m_grepProc = nullptr;
    QVector<SearchResult> m_results;
};
