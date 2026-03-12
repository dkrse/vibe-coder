#pragma once

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

// --- Line number area ---

class CodeEditor;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *m_editor;
};

// --- Syntax highlighter ---

struct HighlightRule {
    QRegularExpression pattern;
    QTextCharFormat format;
};

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

    void setLanguage(const QString &lang);
    void setDarkTheme(bool dark);

protected:
    void highlightBlock(const QString &text) override;

private:
    void buildRules();

    QVector<HighlightRule> m_rules;
    QString m_language;
    bool m_dark = false;

    QRegularExpression m_commentStartExpr;
    QRegularExpression m_commentEndExpr;
    QTextCharFormat m_multiLineCommentFormat;
};

// --- Code editor with line numbers ---

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    void setShowLineNumbers(bool show);
    bool showLineNumbers() const { return m_showLineNumbers; }

    void setEditorColorScheme(const QString &scheme);
    SyntaxHighlighter *highlighter() const { return m_highlighter; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    LineNumberArea *m_lineNumberArea;
    SyntaxHighlighter *m_highlighter;
    bool m_showLineNumbers = true;
};
