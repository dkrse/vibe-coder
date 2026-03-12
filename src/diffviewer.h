#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QPushButton>
#include <QComboBox>

class DiffHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit DiffHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;
};

class DiffViewer : public QWidget {
    Q_OBJECT
public:
    explicit DiffViewer(QWidget *parent = nullptr);

    void refresh(const QString &workDir);
    void setDiffText(const QString &text);
    void setViewerFont(const QFont &font);
    void setViewerColors(const QColor &bg, const QColor &fg);

private:
    QPlainTextEdit *m_textEdit;
    DiffHighlighter *m_highlighter;
    QPushButton *m_refreshBtn;
    QComboBox *m_modeCombo;
    QString m_workDir;
};
