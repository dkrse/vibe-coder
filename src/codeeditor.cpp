#include "codeeditor.h"

#include <QPainter>
#include <QTextBlock>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QStyleOptionSlider>

// ── SpacedDocumentLayout ────────────────────────────────────────────

QRectF SpacedDocumentLayout::blockBoundingRect(const QTextBlock &block) const
{
    QRectF r = QPlainTextDocumentLayout::blockBoundingRect(block);
    if (m_factor > 1.01)
        r.setHeight(r.height() * m_factor);
    return r;
}

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

// ── MarkerScrollBar ─────────────────────────────────────────────────

MarkerScrollBar::MarkerScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent)
{
}

void MarkerScrollBar::setMatchPositions(const QVector<double> &positions)
{
    m_positions = positions;
    update();
}

void MarkerScrollBar::paintEvent(QPaintEvent *event)
{
    QScrollBar::paintEvent(event);

    if (m_positions.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Get the groove area (skip buttons at top/bottom)
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect grooveRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, this);

    QColor markerColor(255, 230, 0, 220);

    for (double pos : m_positions) {
        int y = grooveRect.top() + static_cast<int>(pos * (grooveRect.height() - 2));
        p.fillRect(grooveRect.left() + 1, y, grooveRect.width() - 2, 2, markerColor);
    }
}

// ── FindReplaceBar ──────────────────────────────────────────────────

FindReplaceBar::FindReplaceBar(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    // Search row
    auto *searchRow = new QHBoxLayout;
    searchRow->setSpacing(4);

    m_toggleReplaceBtn = new QPushButton("▶");
    m_toggleReplaceBtn->setFixedSize(20, 22);
    m_toggleReplaceBtn->setFlat(true);
    m_toggleReplaceBtn->setToolTip("Toggle Replace");
    connect(m_toggleReplaceBtn, &QPushButton::clicked, this, [this]() {
        m_replaceVisible = !m_replaceVisible;
        updateReplaceVisibility();
    });

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Find...");
    m_searchEdit->setMinimumWidth(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FindReplaceBar::searchChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::findNext);

    m_matchLabel = new QLabel;
    m_matchLabel->setMinimumWidth(60);

    auto *prevBtn = new QPushButton("▲");
    prevBtn->setFixedSize(24, 22);
    prevBtn->setToolTip("Previous match (Shift+Enter)");
    connect(prevBtn, &QPushButton::clicked, this, &FindReplaceBar::findPrev);

    auto *nextBtn = new QPushButton("▼");
    nextBtn->setFixedSize(24, 22);
    nextBtn->setToolTip("Next match (Enter)");
    connect(nextBtn, &QPushButton::clicked, this, &FindReplaceBar::findNext);

    auto *closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(24, 22);
    closeBtn->setFlat(true);
    connect(closeBtn, &QPushButton::clicked, this, &FindReplaceBar::closed);

    searchRow->addWidget(m_toggleReplaceBtn);
    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(m_matchLabel);
    searchRow->addWidget(prevBtn);
    searchRow->addWidget(nextBtn);
    searchRow->addWidget(closeBtn);

    mainLayout->addLayout(searchRow);

    // Replace row
    auto *replaceRow = new QHBoxLayout;
    replaceRow->setSpacing(4);

    replaceRow->addSpacing(24);
    m_replaceEdit = new QLineEdit;
    m_replaceEdit->setPlaceholderText("Replace...");
    replaceRow->addWidget(m_replaceEdit, 1);

    m_replaceBtn = new QPushButton("Replace");
    m_replaceBtn->setFixedHeight(22);
    connect(m_replaceBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceOne);

    m_replaceAllBtn = new QPushButton("Replace All");
    m_replaceAllBtn->setFixedHeight(22);
    connect(m_replaceAllBtn, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);

    replaceRow->addWidget(m_replaceBtn);
    replaceRow->addWidget(m_replaceAllBtn);
    replaceRow->addSpacing(24);

    mainLayout->addLayout(replaceRow);

    m_replaceVisible = false;
    updateReplaceVisibility();
    setDarkTheme(true);
}

QString FindReplaceBar::searchText() const { return m_searchEdit->text(); }
QString FindReplaceBar::replaceText() const { return m_replaceEdit->text(); }

void FindReplaceBar::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void FindReplaceBar::setShowReplace(bool show)
{
    m_replaceVisible = show;
    updateReplaceVisibility();
}

void FindReplaceBar::setMatchInfo(int current, int total)
{
    if (total == 0)
        m_matchLabel->setText("No results");
    else
        m_matchLabel->setText(QString("%1 of %2").arg(current).arg(total));
}

void FindReplaceBar::updateReplaceVisibility()
{
    m_replaceEdit->setVisible(m_replaceVisible);
    m_replaceBtn->setVisible(m_replaceVisible);
    m_replaceAllBtn->setVisible(m_replaceVisible);
    m_toggleReplaceBtn->setText(m_replaceVisible ? "▼" : "▶");
}

void FindReplaceBar::setDarkTheme(bool)
{
    // Colors are now handled by the global theme stylesheet
}

// ── SyntaxHighlighter ───────────────────────────────────────────────

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_searchFormat.setBackground(QColor(255, 255, 0, 100));
    m_searchFormat.setForeground(Qt::black);
}

void SyntaxHighlighter::setLanguage(const QString &lang)
{
    m_language = lang.toLower();
    buildRules();
}

void SyntaxHighlighter::setDarkTheme(bool dark)
{
    m_dark = dark;
    buildRules();
    rehighlight();
}

void SyntaxHighlighter::setSearchPattern(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    rehighlight();
}

void SyntaxHighlighter::buildRules()
{
    m_rules.clear();

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

        m_rules.append({QRegularExpression("^\\s*#\\s*\\w+.*"), preprocFmt});

        m_commentStartExpr = QRegularExpression("/\\*");
        m_commentEndExpr = QRegularExpression("\\*/");
    }
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
    else if (m_language == "md") {
        QColor headingColor = m_dark ? QColor("#e5c07b") : QColor("#005cc5");
        QColor boldColor    = m_dark ? QColor("#e06c75") : QColor("#d73a49");
        QColor italicColor  = m_dark ? QColor("#c678dd") : QColor("#6f42c1");
        QColor linkColor    = m_dark ? QColor("#61afef") : QColor("#0366d6");
        QColor codeColor    = m_dark ? QColor("#98c379") : QColor("#22863a");
        QColor listColor    = m_dark ? QColor("#d19a66") : QColor("#e36209");
        QColor hrColor      = m_dark ? QColor("#5c6370") : QColor("#959da5");
        QColor blockqColor  = m_dark ? QColor("#5c6370") : QColor("#6a737d");
        QColor mathColor    = m_dark ? QColor("#56b6c2") : QColor("#005cc5");

        QTextCharFormat headFmt;
        headFmt.setForeground(headingColor);
        headFmt.setFontWeight(QFont::Bold);
        // Headings: # ## ### etc.
        m_rules.append({QRegularExpression("^#{1,6}\\s+.*"), headFmt});

        QTextCharFormat boldFmt;
        boldFmt.setForeground(boldColor);
        boldFmt.setFontWeight(QFont::Bold);
        m_rules.append({QRegularExpression("\\*\\*[^*]+\\*\\*"), boldFmt});
        m_rules.append({QRegularExpression("__[^_]+__"), boldFmt});

        QTextCharFormat italFmt;
        italFmt.setForeground(italicColor);
        italFmt.setFontItalic(true);
        m_rules.append({QRegularExpression("(?<!\\*)\\*[^*]+\\*(?!\\*)"), italFmt});
        m_rules.append({QRegularExpression("(?<!_)_[^_]+_(?!_)"), italFmt});

        QTextCharFormat codeFmt;
        codeFmt.setForeground(codeColor);
        // Inline code `...`
        m_rules.append({QRegularExpression("`[^`]+`"), codeFmt});
        // Code fence markers ```
        m_rules.append({QRegularExpression("^```.*"), codeFmt});

        QTextCharFormat linkFmt;
        linkFmt.setForeground(linkColor);
        linkFmt.setFontUnderline(true);
        // Links [text](url) and images ![alt](url)
        m_rules.append({QRegularExpression("!?\\[[^\\]]*\\]\\([^)]*\\)"), linkFmt});
        // Reference links [text][ref]
        m_rules.append({QRegularExpression("\\[[^\\]]*\\]\\[[^\\]]*\\]"), linkFmt});
        // Bare URLs
        m_rules.append({QRegularExpression("https?://[^\\s)>]+"), linkFmt});

        QTextCharFormat listFmt;
        listFmt.setForeground(listColor);
        // Unordered list markers
        m_rules.append({QRegularExpression("^\\s*[*+-]\\s"), listFmt});
        // Ordered list markers
        m_rules.append({QRegularExpression("^\\s*\\d+\\.\\s"), listFmt});

        QTextCharFormat hrFmt;
        hrFmt.setForeground(hrColor);
        m_rules.append({QRegularExpression("^[-*_]{3,}\\s*$"), hrFmt});

        QTextCharFormat bqFmt;
        bqFmt.setForeground(blockqColor);
        bqFmt.setFontItalic(true);
        m_rules.append({QRegularExpression("^>+\\s.*"), bqFmt});

        QTextCharFormat mathFmt;
        mathFmt.setForeground(mathColor);
        // Display math $$...$$
        m_rules.append({QRegularExpression("\\$\\$[^$]+\\$\\$"), mathFmt});
        // Inline math $...$
        m_rules.append({QRegularExpression("(?<!\\$)\\$(?!\\s)[^$]+(?<!\\s)\\$(?!\\$)"), mathFmt});

        QTextCharFormat strikeFmt;
        strikeFmt.setForeground(hrColor);
        m_rules.append({QRegularExpression("~~[^~]+~~"), strikeFmt});

        m_commentStartExpr = QRegularExpression();
        m_commentEndExpr = QRegularExpression();
    }

    // Common rules (skip for markdown — has its own complete ruleset)
    if (m_language != "md") {
        m_rules.append({QRegularExpression("\\b([a-zA-Z_]\\w*)\\s*(?=\\()"), funcFmt});
        m_rules.append({QRegularExpression("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b"), numFmt});
        m_rules.append({QRegularExpression("\\b0[xX][0-9a-fA-F]+\\b"), numFmt});
        m_rules.append({QRegularExpression("\"([^\"\\\\]|\\\\.)*\""), strFmt});
        m_rules.append({QRegularExpression("'([^'\\\\]|\\\\.)*'"), strFmt});
        m_rules.append({QRegularExpression("//[^\n]*"), commentFmt});
        if (m_language == "py")
            m_rules.append({QRegularExpression("#[^\n]*"), commentFmt});
    }
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
    if (m_commentStartExpr.isValid() && !m_commentStartExpr.pattern().isEmpty()) {
        setCurrentBlockState(0);

        int startIndex = 0;
        bool continuing = (previousBlockState() == 1);
        if (!continuing)
            startIndex = text.indexOf(m_commentStartExpr);

        while (startIndex >= 0) {
            // When continuing from previous block, search from startIndex;
            // when we just found /*, skip past it (+2) to avoid matching /* as */
            int searchFrom = continuing ? startIndex : startIndex + 2;
            auto endMatch = m_commentEndExpr.match(text, searchFrom);
            int endIndex = endMatch.capturedStart();
            int commentLength;
            if (endIndex == -1 || !endMatch.hasMatch()) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex + endMatch.capturedLength();
            }
            setFormat(startIndex, commentLength, m_multiLineCommentFormat);
            continuing = false;
            startIndex = text.indexOf(m_commentStartExpr, startIndex + commentLength);
        }
    }

    // Search highlight (on top of everything)
    if (!m_searchText.isEmpty()) {
        int idx = 0;
        while ((idx = text.indexOf(m_searchText, idx, Qt::CaseInsensitive)) != -1) {
            setFormat(idx, m_searchText.length(), m_searchFormat);
            idx += m_searchText.length();
        }
    }
}

// ── CodeEditor ──────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_spacedLayout = new SpacedDocumentLayout(document());
    document()->setDocumentLayout(m_spacedLayout);
    m_lineNumberArea = new LineNumberArea(this);
    m_highlighter = new SyntaxHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &CodeEditor::updateLineNumberArea);

    updateLineNumberAreaWidth(0);

    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::updateCurrentLineHighlight);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightMatchingBracket);

    // Replace default vertical scrollbar with our marker scrollbar
    m_markerScrollBar = new MarkerScrollBar(Qt::Vertical, this);
    setVerticalScrollBar(m_markerScrollBar);

    // Search debounce timer
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(200);
    connect(m_searchDebounce, &QTimer::timeout, this, &CodeEditor::doSearch);

    // Find bar
    m_findBar = new FindReplaceBar(this);
    m_findBar->setVisible(false);

    connect(m_findBar, &FindReplaceBar::searchChanged, this, &CodeEditor::onSearchChanged);
    connect(m_findBar, &FindReplaceBar::findNext, this, &CodeEditor::onFindNext);
    connect(m_findBar, &FindReplaceBar::findPrev, this, &CodeEditor::onFindPrev);
    connect(m_findBar, &FindReplaceBar::replaceOne, this, &CodeEditor::onReplaceOne);
    connect(m_findBar, &FindReplaceBar::replaceAll, this, &CodeEditor::onReplaceAll);
    connect(m_findBar, &FindReplaceBar::closed, this, &CodeEditor::hideFindBar);
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
    positionFindBar();
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_F) {
        showFindBar(false);
        return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_H) {
        showFindBar(true);
        return;
    }
    if (event->key() == Qt::Key_Escape && m_findBar->isVisible()) {
        hideFindBar();
        return;
    }

    // Auto-close brackets
    if (m_autoClose && event->modifiers() == Qt::NoModifier) {
        static const QHash<QChar, QChar> pairs = {
            {'(', ')'}, {'{', '}'}, {'[', ']'}, {'"', '"'}, {'\'', '\''}
        };
        QChar ch = event->text().isEmpty() ? QChar() : event->text()[0];

        // If typing a closing bracket that already follows cursor, just skip over it
        if (ch == ')' || ch == '}' || ch == ']' || ch == '"' || ch == '\'') {
            QTextCursor cur = textCursor();
            if (!cur.atEnd()) {
                QChar next = document()->characterAt(cur.position());
                if (next == ch) {
                    // For quotes, only skip if this char is also in pairs values
                    cur.movePosition(QTextCursor::Right);
                    setTextCursor(cur);
                    return;
                }
            }
        }

        if (pairs.contains(ch)) {
            QChar close = pairs[ch];
            QTextCursor cur = textCursor();
            int pos = cur.position();
            // Insert both chars, position cursor between them
            cur.beginEditBlock();
            cur.insertText(QString(ch) + QString(close));
            cur.endEditBlock();
            cur.setPosition(pos + 1);
            setTextCursor(cur);
            return;
        }

        // Backspace: delete matching pair if cursor is between them
        if (event->key() == Qt::Key_Backspace) {
            QTextCursor cur = textCursor();
            if (!cur.atStart() && !cur.atEnd()) {
                QChar before = document()->characterAt(cur.position() - 1);
                QChar after = document()->characterAt(cur.position());
                if ((before == '(' && after == ')') ||
                    (before == '{' && after == '}') ||
                    (before == '[' && after == ']') ||
                    (before == '"' && after == '"') ||
                    (before == '\'' && after == '\'')) {
                    cur.beginEditBlock();
                    cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 1);
                    cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                    cur.removeSelectedText();
                    cur.endEditBlock();
                    setTextCursor(cur);
                    return;
                }
            }
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!m_showLineNumbers) return;

    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    QColor numColor = palette().color(QPalette::PlaceholderText);
    painter.setPen(numColor);
    int areaWidth = m_lineNumberArea->width() - 2;
    int lineHeight = fontMetrics().height();
    QString number;

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            number.setNum(blockNumber + 1);
            painter.drawText(0, top, areaWidth, lineHeight, Qt::AlignRight, number);
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
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    m_lineNumberArea->update();
}

void CodeEditor::setEditorColorScheme(const QString &scheme, const QColor &bg, const QColor &fg)
{
    bool dark = scheme.toLower().contains("dark");
    m_darkScheme = dark;
    m_highlighter->setDarkTheme(dark);
    m_findBar->setDarkTheme(dark);

    QColor baseBg = bg.isValid() ? bg : (dark ? QColor("#1e1e1e") : QColor("#ffffff"));
    QColor baseFg = fg.isValid() ? fg : (dark ? QColor("#d4d4d4") : QColor("#333333"));
    QColor altBg = dark ? baseBg.lighter(115) : baseBg.darker(105);
    QColor placeholderFg = dark ? baseFg.darker(160) : baseFg.lighter(200);

    QPalette pal = palette();
    pal.setColor(QPalette::Base, baseBg);
    pal.setColor(QPalette::Text, baseFg);
    pal.setColor(QPalette::AlternateBase, altBg);
    pal.setColor(QPalette::PlaceholderText, placeholderFg);
    setPalette(pal);

    // Override global stylesheet for line number area
    m_lineNumberArea->setStyleSheet(
        QString("background-color: %1; color: %2;").arg(altBg.name(), placeholderFg.name()));
    m_lineNumberArea->update();

    updateCurrentLineHighlight();
}

void CodeEditor::setHighlightCurrentLine(bool enable)
{
    m_highlightLine = enable;
    updateCurrentLineHighlight();
}

void CodeEditor::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (m_highlightLine) {
        QTextEdit::ExtraSelection sel;
        QColor lineColor = m_lineHighlightColor.isValid() ? m_lineHighlightColor
            : (m_darkScheme ? QColor("#2a2d2e") : QColor("#e8f2fe"));
        sel.format.setBackground(lineColor);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        selections.append(sel);
    }
    setExtraSelections(selections);
}

void CodeEditor::setLineSpacing(double factor)
{
    m_lineSpacing = factor;
    m_spacedLayout->setSpacingFactor(factor);
}

void CodeEditor::setLargeFile(bool large)
{
    m_largeFile = large;
    if (large) {
        m_highlighter->setDocument(nullptr); // disable syntax highlighting
        setLineWrapMode(QPlainTextEdit::NoWrap); // no wrapping for performance
    }
}

// ── Find/Replace implementation ─────────────────────────────────────

void CodeEditor::showFindBar(bool withReplace)
{
    m_findBar->setShowReplace(withReplace);
    m_findBar->setVisible(true);
    positionFindBar();
    m_findBar->focusSearch();
}

void CodeEditor::hideFindBar()
{
    m_findBar->setVisible(false);
    m_highlighter->setSearchPattern(QString());
    m_markerScrollBar->setMatchPositions({});
    m_currentMatch = -1;
    m_totalMatches = 0;
    setFocus();
}

void CodeEditor::positionFindBar()
{
    int barWidth = qMin(width() - 20, 500);
    int barHeight = m_findBar->sizeHint().height();
    m_findBar->setGeometry(width() - barWidth - 10, 0, barWidth, barHeight);
    m_findBar->raise();
}

void CodeEditor::onSearchChanged(const QString &text)
{
    m_pendingSearch = text;
    if (text.isEmpty()) {
        m_searchDebounce->stop();
        doSearch();
    } else {
        m_searchDebounce->start();
    }
}

void CodeEditor::doSearch()
{
    const QString &text = m_pendingSearch;
    m_highlighter->setSearchPattern(text);
    m_totalMatches = 0;
    m_currentMatch = -1;

    if (text.isEmpty()) {
        m_findBar->setMatchInfo(0, 0);
        m_markerScrollBar->setMatchPositions({});
        return;
    }

    // Find all matches and collect block positions for scrollbar
    int totalBlocks = blockCount();
    int textLen = text.length();
    QTextBlock block = document()->begin();
    QVector<double> scrollPositions;
    scrollPositions.reserve(256);

    while (block.isValid()) {
        QString blockText = block.text();
        int idx = 0;
        bool hasMatch = false;
        while ((idx = blockText.indexOf(text, idx, Qt::CaseInsensitive)) != -1) {
            m_totalMatches++;
            hasMatch = true;
            idx += textLen;
        }
        if (hasMatch && totalBlocks > 0) {
            double pos = static_cast<double>(block.blockNumber()) / totalBlocks;
            scrollPositions.append(pos);
        }
        block = block.next();
    }

    m_markerScrollBar->setMatchPositions(scrollPositions);

    if (m_totalMatches > 0) {
        m_currentMatch = 0;
        onFindNext();
    } else {
        m_findBar->setMatchInfo(0, 0);
    }
}

void CodeEditor::onFindNext()
{
    QString text = m_findBar->searchText();
    if (text.isEmpty() || m_totalMatches == 0) return;

    QTextCursor cur = textCursor();
    QTextCursor found = document()->find(text, cur);
    if (found.isNull()) {
        // Wrap around
        cur.movePosition(QTextCursor::Start);
        found = document()->find(text, cur);
    }

    if (!found.isNull()) {
        setTextCursor(found);
        ensureCursorVisible();
        m_currentMatch++;
        if (m_currentMatch >= m_totalMatches)
            m_currentMatch = 0;
        m_findBar->setMatchInfo(m_currentMatch + 1, m_totalMatches);
    }
}

void CodeEditor::onFindPrev()
{
    QString text = m_findBar->searchText();
    if (text.isEmpty() || m_totalMatches == 0) return;

    QTextCursor cur = textCursor();
    if (cur.hasSelection())
        cur.setPosition(cur.selectionStart());

    QTextCursor found = document()->find(text, cur, QTextDocument::FindBackward);
    if (found.isNull()) {
        cur.movePosition(QTextCursor::End);
        found = document()->find(text, cur, QTextDocument::FindBackward);
    }

    if (!found.isNull()) {
        setTextCursor(found);
        ensureCursorVisible();
        m_currentMatch--;
        if (m_currentMatch < 0)
            m_currentMatch = m_totalMatches - 1;
        m_findBar->setMatchInfo(m_currentMatch + 1, m_totalMatches);
    }
}

void CodeEditor::onReplaceOne()
{
    QString searchText = m_findBar->searchText();
    QString replaceText = m_findBar->replaceText();
    if (searchText.isEmpty()) return;

    QTextCursor cur = textCursor();
    if (cur.hasSelection() && cur.selectedText().compare(searchText, Qt::CaseInsensitive) == 0) {
        cur.insertText(replaceText);
        setTextCursor(cur);
    }
    onSearchChanged(searchText);
    onFindNext();
}

void CodeEditor::onReplaceAll()
{
    QString searchText = m_findBar->searchText();
    QString replaceText = m_findBar->replaceText();
    if (searchText.isEmpty()) return;

    QTextCursor cur = textCursor();
    cur.beginEditBlock();
    cur.movePosition(QTextCursor::Start);

    while (true) {
        QTextCursor found = document()->find(searchText, cur);
        if (found.isNull()) break;
        found.insertText(replaceText);
        cur = found;
    }

    cur.endEditBlock();
    onSearchChanged(searchText);
}

// ── Bracket matching ─────────────────────────────────────────────────

void CodeEditor::setBracketMatching(bool enable)
{
    m_bracketMatching = enable;
    if (!enable) {
        // Clear bracket highlights but keep current line highlight
        updateCurrentLineHighlight();
    }
}

void CodeEditor::setAutoCloseBrackets(bool enable)
{
    m_autoClose = enable;
}

int CodeEditor::findMatchingBracket(int pos, QChar open, QChar close, bool forward) const
{
    int depth = 1;
    int len = document()->characterCount();
    int i = forward ? pos + 1 : pos - 1;

    while (i >= 0 && i < len && depth > 0) {
        QChar ch = document()->characterAt(i);
        if (ch == open) depth += (forward ? 1 : -1);
        if (ch == close) depth += (forward ? -1 : 1);
        if (depth == 0) return i;
        i += forward ? 1 : -1;
    }
    return -1;
}

void CodeEditor::highlightMatchingBracket()
{
    if (!m_bracketMatching) return;

    // Start from current extra selections (current line highlight)
    QList<QTextEdit::ExtraSelection> selections = extraSelections();

    QTextCursor cur = textCursor();
    int pos = cur.position();

    static const QHash<QChar, QChar> openToClose = {{'(', ')'}, {'{', '}'}, {'[', ']'}};
    static const QHash<QChar, QChar> closeToOpen = {{')', '('}, {'}', '{'}, {']', '['}};

    auto tryMatch = [&](int checkPos) -> bool {
        if (checkPos < 0 || checkPos >= document()->characterCount()) return false;
        QChar ch = document()->characterAt(checkPos);

        int matchPos = -1;
        if (openToClose.contains(ch))
            matchPos = findMatchingBracket(checkPos, ch, openToClose[ch], true);
        else if (closeToOpen.contains(ch))
            matchPos = findMatchingBracket(checkPos, closeToOpen[ch], ch, false);

        if (matchPos >= 0) {
            QColor bracketColor = m_darkScheme ? QColor("#444444") : QColor("#d0d0d0");

            QTextEdit::ExtraSelection sel1;
            sel1.format.setBackground(bracketColor);
            sel1.cursor = QTextCursor(document());
            sel1.cursor.setPosition(checkPos);
            sel1.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            selections.append(sel1);

            QTextEdit::ExtraSelection sel2;
            sel2.format.setBackground(bracketColor);
            sel2.cursor = QTextCursor(document());
            sel2.cursor.setPosition(matchPos);
            sel2.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            selections.append(sel2);

            return true;
        }
        return false;
    };

    // Check character at cursor, then before cursor
    if (!tryMatch(pos))
        tryMatch(pos - 1);

    setExtraSelections(selections);
}
