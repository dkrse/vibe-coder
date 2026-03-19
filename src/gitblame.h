#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QLabel>
#include <QPushButton>

struct BlameLine {
    QString hash;
    QString author;
    QString date;
    int lineNum;
    QString text;
};

class BlameView;

class BlameGutter : public QWidget {
    Q_OBJECT
public:
    explicit BlameGutter(BlameView *view);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    BlameView *m_view;
};

class BlameView : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit BlameView(QWidget *parent = nullptr);
    void setBlameData(const QVector<BlameLine> &lines);
    void setViewerColors(const QColor &bg, const QColor &fg);

    void gutterPaintEvent(QPaintEvent *event);
    int gutterWidth() const;

    QColor bgColor() const { return m_bgColor; }
    QColor fgColor() const { return m_fgColor; }
    const QVector<BlameLine> &blameLines() const { return m_lines; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateGutterWidth();
    void updateGutter(const QRect &rect, int dy);

    QVector<BlameLine> m_lines;
    BlameGutter *m_gutter;
    QColor m_bgColor = QColor("#1e1e1e");
    QColor m_fgColor = QColor("#d4d4d4");
};

class GitBlame : public QWidget {
    Q_OBJECT
public:
    explicit GitBlame(QWidget *parent = nullptr);

    void blameFile(const QString &workDir, const QString &filePath);
    void setViewerFont(const QFont &font);
    void setViewerColors(const QColor &bg, const QColor &fg);

signals:
    void outputMessage(const QString &msg, int level);

private:
    void onBlameFinished(int exitCode, QProcess::ExitStatus status);

    BlameView *m_blameView;
    QLabel *m_fileLabel;
    QPushButton *m_refreshBtn;
    QString m_workDir;
    QString m_filePath;
    QProcess *m_proc = nullptr;
};
