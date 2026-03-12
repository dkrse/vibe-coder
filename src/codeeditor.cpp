#include "codeeditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QFileInfo>

// ── LineNumberArea ──────────────────────────────────────────────────

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor), m_editor(editor) {}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_editor->lineNumberAreaPaintEvent(event);
}

// ── SyntaxHighlighter ───────────────────────────────────────────────

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
}

void SyntaxHighlighter::setLanguage(const QString &lang)
{
    m_language = lang.toLower();
    buildRules();
    rehighlight();
}

void SyntaxHighlighter::setDarkTheme(bool dark)
{
    m_dark = dark;
    buildRules();
    rehighlight();
}

void SyntaxHighlighter::buildRules()
{
    m_rules.clear();

    // Colors depending on theme
    QColor keywordColor   = m_dark ? QColor("#569cd6") : QColor("#0000ff");
    QColor typeColor      = m_dark ? QColor("#4ec9b0") : QColor("#267f99");
    QColor stringColor    = m_dark ? QColor("#ce9178") : QColor("#a31515");
    QColor commentColor   = m_dark ? QColor("#6a9955") : QColor("#008000");
    QColor numberColor    = m_dark ? QColor("#b5cea8") : QColor("#098658");
    QColor preprocessColor= m_dark ? QColor("#c586c0") : QColor("#af00db");
    QColor funcColor      = m_dark ? QColor("#dcdcaa") : QColor("#795e26");

    QTextCharFormat kwFmt;
    kwFmt.setForeground(keywordColor);
    kwFmt.setFontWeight(QFont::Bold);

    QTextCharFormat typeFmt;
    typeFmt.setForeground(typeColor);

    QTextCharFormat strFmt;
    strFmt.setForeground(stringColor);

    QTextCharFormat commentFmt;
    commentFmt.setForeground(commentColor);
    commentFmt.setFontItalic(true);

    QTextCharFormat numFmt;
    numFmt.setForeground(numberColor);

    QTextCharFormat preprocFmt;
    preprocFmt.setForeground(preprocessColor);

    QTextCharFormat funcFmt;
    funcFmt.setForeground(funcColor);

    m_multiLineCommentFormat = commentFmt;

    // C/C++ keywords
    if (m_language == "c" || m_language == "cpp" || m_language == "h" || m_language == "hpp") {
        QStringList keywords = {
            "auto", "break", "case", "catch", "class", "const", "constexpr",
            "continue", "default", "delete", "do", "else", "enum", "explicit",
            "extern", "false", "for", "friend", "goto", "if", "inline",
            "namespace", "new", "noexcept", "nullptr", "operator", "override",
            "private", "protected", "public", "return", "sizeof", "static",
            "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",
            "struct", "switch", "template", "this", "throw", "true", "try",
            "typedef", "typeid", "typename", "union", "using", "virtual",
            "void", "volatile", "while", "final"
        };
        for (const auto &kw : keywords)
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});

        QStringList types = {
            "int", "long", "short", "float", "double", "char", "unsigned",
            "signed", "bool", "size_t", "string", "vector", "map", "set",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t",
            "int8_t", "int16_t", "int32_t", "int64_t",
            "QString", "QWidget", "QObject"
        };
        for (const auto &t : types)
            m_rules.append({QRegularExpression("\\b" + t + "\\b"), typeFmt});

        // Preprocessor
        m_rules.append({QRegularExpression("^\\s*#\\s*\\w+.*"), preprocFmt});

        m_commentStartExpr = QRegularExpression("/\\*");
        m_commentEndExpr = QRegularExpression("\\*/");
    }
    // Python
    else if (m_language == "py") {
        QStringList keywords = {
            "and", "as", "assert", "async", "await", "break", "class",
            "continue", "def", "del", "elif", "else", "except", "False",
            "finally", "for", "from", "global", "if", "import", "in",
            "is", "lambda", "None", "nonlocal", "not", "or", "pass",
            "raise", "return", "True", "try", "while", "with", "yield"
        };
        for (const auto &kw : keywords)
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});

        m_commentStartExpr = QRegularExpression();
        m_commentEndExpr = QRegularExpression();
    }
    // JavaScript/TypeScript
    else if (m_language == "js" || m_language == "ts" || m_language == "jsx" || m_language == "tsx") {
        QStringList keywords = {
            "break", "case", "catch", "class", "const", "continue", "debugger",
            "default", "delete", "do", "else", "export", "extends", "false",
            "finally", "for", "function", "if", "import", "in", "instanceof",
            "let", "new", "null", "return", "super", "switch", "this",
            "throw", "true", "try", "typeof", "undefined", "var", "void",
            "while", "with", "yield", "async", "await", "of"
        };
        for (const auto &kw : keywords)
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});

        m_commentStartExpr = QRegularExpression("/\\*");
        m_commentEndExpr = QRegularExpression("\\*/");
    }
    // Rust
    else if (m_language == "rs") {
        QStringList keywords = {
            "as", "break", "const", "continue", "crate", "else", "enum",
            "extern", "false", "fn", "for", "if", "impl", "in", "let",
            "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
            "self", "Self", "static", "struct", "super", "trait", "true",
            "type", "unsafe", "use", "where", "while", "async", "await"
        };
        for (const auto &kw : keywords)
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), kwFmt});

        m_commentStartExpr = QRegularExpression("/\\*");
        m_commentEndExpr = QRegularExpression("\\*/");
    }

    // Common rules for all languages

    // Function calls
    m_rules.append({QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*(?=\\()"), funcFmt});

    // Numbers
    m_rules.append({QRegularExpression("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b"), numFmt});
    m_rules.append({QRegularExpression("\\b0[xX][0-9a-fA-F]+\\b"), numFmt});

    // Strings
    m_rules.append({QRegularExpression("\"([^\"\\\\]|\\\\.)*\""), strFmt});
    m_rules.append({QRegularExpression("'([^'\\\\]|\\\\.)*'"), strFmt});

    // Single-line comment
    m_rules.append({QRegularExpression("//[^\n]*"), commentFmt});
    // Python comment
    if (m_language == "py")
        m_rules.append({QRegularExpression("#[^\n]*"), commentFmt});
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Multi-line comments
    if (!m_commentStartExpr.isValid() || m_commentStartExpr.pattern().isEmpty())
        return;

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(m_commentStartExpr);

    while (startIndex >= 0) {
        auto endMatch = m_commentEndExpr.match(text, startIndex + 2);
        int endIndex = endMatch.capturedStart();
        int commentLength;
        if (endIndex == -1 || !endMatch.hasMatch()) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, m_multiLineCommentFormat);
        startIndex = text.indexOf(m_commentStartExpr, startIndex + commentLength);
    }
}

// ── CodeEditor ──────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);
    m_highlighter = new SyntaxHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);

    updateLineNumberAreaWidth(0);
}

int CodeEditor::lineNumberAreaWidth()
{
    if (!m_showLineNumbers) return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) { max /= 10; ++digits; }
    return 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * (digits + 1);
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(),
                                         lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!m_showLineNumbers) return;

    QPainter painter(m_lineNumberArea);

    // Use palette colors
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    QColor numColor = palette().color(QPalette::PlaceholderText);

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(numColor);
            painter.drawText(0, top, m_lineNumberArea->width() - 2,
                            fontMetrics().height(), Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::setShowLineNumbers(bool show)
{
    m_showLineNumbers = show;
    m_lineNumberArea->setVisible(show);
    updateLineNumberAreaWidth(0);
}

void CodeEditor::setEditorColorScheme(const QString &scheme)
{
    bool dark = scheme.toLower().contains("dark");
    m_highlighter->setDarkTheme(dark);

    QPalette pal = palette();
    if (dark) {
        pal.setColor(QPalette::Base, QColor("#1e1e1e"));
        pal.setColor(QPalette::Text, QColor("#d4d4d4"));
        pal.setColor(QPalette::AlternateBase, QColor("#252526"));
        pal.setColor(QPalette::PlaceholderText, QColor("#858585"));
    } else {
        pal.setColor(QPalette::Base, QColor("#ffffff"));
        pal.setColor(QPalette::Text, QColor("#333333"));
        pal.setColor(QPalette::AlternateBase, QColor("#f0f0f0"));
        pal.setColor(QPalette::PlaceholderText, QColor("#999999"));
    }
    setPalette(pal);
}
