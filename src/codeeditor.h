#pragma once

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QPlainTextDocumentLayout>

// --- SpacedDocumentLayout ---

class SpacedDocumentLayout : public QPlainTextDocumentLayout {
    Q_OBJECT
public:
    explicit SpacedDocumentLayout(QTextDocument *doc) : QPlainTextDocumentLayout(doc) {}
    void setSpacingFactor(double f) { m_factor = f; requestUpdate(); }
    double spacingFactor() const { return m_factor; }
    QRectF blockBoundingRect(const QTextBlock &block) const override;
private:
    double m_factor = 1.0;
};

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

// --- Custom scrollbar with match markers ---

class MarkerScrollBar : public QScrollBar {
    Q_OBJECT
public:
    explicit MarkerScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    void setMatchPositions(const QVector<double> &positions); // 0.0–1.0

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_positions;
};

// --- Find/Replace bar ---

class FindReplaceBar : public QWidget {
    Q_OBJECT
public:
    explicit FindReplaceBar(QWidget *parent = nullptr);

    QString searchText() const;
    QString replaceText() const;
    void focusSearch();
    void setShowReplace(bool show);
    void setMatchInfo(int current, int total);
    void setDarkTheme(bool dark);

signals:
    void searchChanged(const QString &text);
    void findNext();
    void findPrev();
    void replaceOne();
    void replaceAll();
    void closed();

private:
    QLineEdit *m_searchEdit;
    QLineEdit *m_replaceEdit;
    QPushButton *m_replaceBtn;
    QPushButton *m_replaceAllBtn;
    QLabel *m_matchLabel;
    QPushButton *m_toggleReplaceBtn;
    bool m_replaceVisible = false;

    void updateReplaceVisibility();
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

    void setSearchPattern(const QString &text);

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

    QString m_searchText;
    QTextCharFormat m_searchFormat;
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

    void setEditorColorScheme(const QString &scheme, const QColor &bg = QColor(), const QColor &fg = QColor());
    SyntaxHighlighter *highlighter() const { return m_highlighter; }
    bool isDarkScheme() const { return m_darkScheme; }

    void setHighlightCurrentLine(bool enable);
    void setLineHighlightColor(const QColor &color) { m_lineHighlightColor = color; }
    void setLineSpacing(double factor);
    bool highlightCurrentLine() const { return m_highlightLine; }

    void setLargeFile(bool large);

    void showFindBar(bool withReplace = false);
    void hideFindBar();

    void setBracketMatching(bool enable);
    bool bracketMatching() const { return m_bracketMatching; }

    void setAutoCloseBrackets(bool enable);
    bool autoCloseBrackets() const { return m_autoClose; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    LineNumberArea *m_lineNumberArea;
    SyntaxHighlighter *m_highlighter;
    bool m_showLineNumbers = true;
    bool m_darkScheme = true;
    bool m_highlightLine = false;
    QColor m_lineHighlightColor;
    double m_lineSpacing = 1.0;
    SpacedDocumentLayout *m_spacedLayout;
    bool m_largeFile = false;
    void updateCurrentLineHighlight();

    FindReplaceBar *m_findBar;
    MarkerScrollBar *m_markerScrollBar;
    QTimer *m_searchDebounce;
    QString m_pendingSearch;

    void onSearchChanged(const QString &text);
    void doSearch();
    void onFindNext();
    void onFindPrev();
    void onReplaceOne();
    void onReplaceAll();
    void positionFindBar();
    int m_currentMatch = -1;
    int m_totalMatches = 0;

    // Bracket matching
    bool m_bracketMatching = true;
    bool m_autoClose = true;
    void highlightMatchingBracket();
    int findMatchingBracket(int pos, QChar open, QChar close, bool forward) const;
};
