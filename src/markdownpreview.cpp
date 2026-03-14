#include "markdownpreview.h"

#include <QVBoxLayout>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QWebEngineSettings>
#include <QWebChannel>
#include <dlfcn.h>
#include <cstdlib>

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    // file:// access needed for loading mermaid.js and preview HTML from temp dir
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    layout->addWidget(m_webView);

    // Extract mermaid.js to temp file
    m_mermaidJsPath = QDir::tempPath() + "/vibe-coder-mermaid.min.js";
    if (!QFile::exists(m_mermaidJsPath)) {
        QFile res(":/js/mermaid.min.js");
        if (res.open(QIODevice::ReadOnly)) {
            QFile out(m_mermaidJsPath);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(res.readAll());
                out.close();
            }
            res.close();
        }
    }

    // Try to load libcmark
    const char *cmarkLibs[] = {
        "libcmark.so", "libcmark.so.0", "libcmark.so.0.31.0",
        "libcmark.so.0.30.3", "libcmark.so.0.30.2", nullptr
    };
    for (int i = 0; cmarkLibs[i] && !m_cmarkLib; ++i)
        m_cmarkLib = dlopen(cmarkLibs[i], RTLD_LAZY);
    if (m_cmarkLib) {
        m_cmarkToHtml = reinterpret_cast<CmarkToHtmlFn>(dlsym(m_cmarkLib, "cmark_markdown_to_html"));
        m_hasCmark = (m_cmarkToHtml != nullptr);
    }

    // 500ms debounce
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &MarkdownPreview::render);

    // When base page finishes loading, render pending content
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        if (ok) {
            m_pageLoaded = true;
            if (!m_pendingMd.isEmpty())
                render();
        }
    });

    // Load the base page immediately (with mermaid.js)
    loadBasePage();
}

MarkdownPreview::~MarkdownPreview()
{
    if (m_cmarkLib) dlclose(m_cmarkLib);
}

void MarkdownPreview::setDarkMode(bool dark)
{
    m_dark = dark;
    // Reload base page with new theme, then re-render
    m_pageLoaded = false;
    loadBasePage();
}

void MarkdownPreview::setFont(const QFont &font)
{
    m_webView->settings()->setFontFamily(QWebEngineSettings::StandardFont, font.family());
    m_webView->settings()->setFontSize(QWebEngineSettings::DefaultFontSize, font.pointSize());
}

void MarkdownPreview::updateContent(const QString &markdown)
{
    m_pendingMd = markdown;
    m_debounce->start();
}

void MarkdownPreview::loadBasePage()
{
    QString bg     = m_dark ? "#1e1e1e" : "#ffffff";
    QString fg     = m_dark ? "#d4d4d4" : "#24292f";
    QString codeBg = m_dark ? "#2d2d2d" : "#f0f0f0";
    QString codeFg = m_dark ? "#ce9178" : "#c7254e";
    QString borderC = m_dark ? "#3e3e3e" : "#d0d0d0";
    QString linkC  = m_dark ? "#61afef" : "#0366d6";
    QString headC  = m_dark ? "#e5c07b" : "#24292f";
    QString bqBorder = m_dark ? "#565656" : "#dfe2e5";
    QString bqFg   = m_dark ? "#9e9e9e" : "#6a737d";
    QString hrC    = m_dark ? "#3e3e3e" : "#e1e4e8";
    QString thBg   = m_dark ? "#2d2d2d" : "#f6f8fa";
    QString mermaidTheme = m_dark ? "dark" : "default";

    QString page = QString(R"(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style>
body {
    background: %1; color: %2;
    font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif;
    font-size: 15px; line-height: 1.6;
    padding: 16px 24px; margin: 0;
    word-wrap: break-word;
}
h1, h2, h3, h4, h5, h6 { color: %7; margin-top: 24px; margin-bottom: 8px; font-weight: 600; }
h1 { font-size: 2em; border-bottom: 1px solid %5; padding-bottom: 0.3em; }
h2 { font-size: 1.5em; border-bottom: 1px solid %5; padding-bottom: 0.3em; }
h3 { font-size: 1.25em; }
p { margin: 8px 0; }
a { color: %6; text-decoration: none; }
a:hover { text-decoration: underline; }
code {
    background: %3; color: %4;
    padding: 2px 6px; border-radius: 3px;
    font-family: 'JetBrains Mono', 'Fira Code', monospace;
    font-size: 0.9em;
}
pre {
    background: %3; padding: 12px 16px;
    border-radius: 6px; overflow-x: auto;
    border: 1px solid %5;
}
pre code { background: none; padding: 0; color: %2; font-size: 0.85em; }
pre.mermaid { background: transparent; border: none; padding: 8px 0; text-align: center; }
blockquote { border-left: 4px solid %8; color: %9; margin: 8px 0; padding: 4px 16px; }
ul, ol { padding-left: 24px; margin: 8px 0; }
li { margin: 4px 0; }
hr { border: none; border-top: 1px solid %10; margin: 24px 0; }
table { border-collapse: collapse; width: 100%%; margin: 8px 0; }
th, td { border: 1px solid %5; padding: 6px 12px; text-align: left; }
th { background: %11; font-weight: 600; }
img { max-width: 100%%; border-radius: 4px; }
del { text-decoration: line-through; opacity: 0.6; }
</style>
<script src="file://%12"></script>
<script>
var mermaidTheme = '%13';
function updateBody(html) {
    document.getElementById('content').innerHTML = html;
    // Decode HTML entities in mermaid blocks
    document.querySelectorAll('pre.mermaid').forEach(function(el) {
        var tmp = document.createElement('textarea');
        tmp.innerHTML = el.innerHTML;
        el.textContent = tmp.value;
    });
    // Re-run mermaid on new content
    if (document.querySelectorAll('pre.mermaid').length > 0) {
        mermaid.initialize({ startOnLoad: false, theme: mermaidTheme, securityLevel: 'loose',
            flowchart: { useMaxWidth: true, htmlLabels: true } });
        mermaid.run({ querySelector: 'pre.mermaid' });
    }
}
</script>
</head><body>
<div id="content"></div>
</body></html>)")
        .arg(bg, fg, codeBg, codeFg, borderC, linkC, headC,
             bqBorder, bqFg, hrC, thBg,
             m_mermaidJsPath, mermaidTheme);

    // Write base page to file (avoids setHtml 2MB limit)
    QString tmpPath = QDir::tempPath() + "/vibe-coder-preview.html";
    QFile f(tmpPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(page.toUtf8());
        f.close();
    }
    m_webView->load(QUrl::fromLocalFile(tmpPath));
}

void MarkdownPreview::render()
{
    if (!m_pageLoaded) return; // base page not ready yet

    QString html = markdownToHtml(m_pendingMd);

    // Convert mermaid code blocks to <pre class="mermaid">
    html.replace(QRegularExpression(
        R"(<pre><code class="language-mermaid">([\s\S]*?)</code></pre>)"),
        R"(<pre class="mermaid">\1</pre>)");

    // Escape for JS string (backslashes, quotes, newlines)
    html.replace('\\', "\\\\");
    html.replace('\'', "\\'");
    html.replace('\n', "\\n");
    html.replace('\r', "");

    // Call JS to update body — no page reload
    QString js = QString("updateBody('%1');").arg(html);
    m_webView->page()->runJavaScript(js);
}

// ── Markdown → HTML ─────────────────────────────────────────────────

QString MarkdownPreview::markdownToHtml(const QString &md)
{
    if (m_hasCmark)
        return cmarkConvert(md);
    return regexConvert(md);
}

QString MarkdownPreview::cmarkConvert(const QString &md)
{
    QByteArray utf8 = md.toUtf8();
    char *result = m_cmarkToHtml(utf8.constData(), utf8.size(), (1 << 17));
    if (!result) return regexConvert(md);
    QString html = QString::fromUtf8(result);
    free(result);
    return html;
}

QString MarkdownPreview::regexConvert(const QString &md)
{
    QString html;
    QStringList lines = md.split('\n');

    bool inCodeBlock = false;
    QString codeBlockLang;
    QString codeContent;
    bool inList = false;
    bool inOrderedList = false;
    bool inBlockquote = false;
    bool inTable = false;

    auto escapeHtml = [](const QString &s) {
        QString r = s;
        r.replace('&', "&amp;");
        r.replace('<', "&lt;");
        r.replace('>', "&gt;");
        return r;
    };

    // Pre-compiled regex patterns for inline processing
    static QRegularExpression reImg(R"(!\[([^\]]*)\]\(([^)]+)\))");
    static QRegularExpression reLink(R"(\[([^\]]+)\]\(([^)]+)\))");
    static QRegularExpression reBoldItalic(R"(\*{3}(.+?)\*{3})");
    static QRegularExpression reBold(R"(\*{2}(.+?)\*{2})");
    static QRegularExpression reItalic(R"(\*(.+?)\*)");
    static QRegularExpression reStrike(R"(~~(.+?)~~)");
    static QRegularExpression reCode(R"(`([^`]+)`)");

    auto processInline = [&escapeHtml](const QString &line) -> QString {
        QString s = escapeHtml(line);
        s.replace(reImg, R"(<img src="\2" alt="\1" style="max-width:100%;">)");
        s.replace(reLink, R"(<a href="\2">\1</a>)");
        s.replace(reBoldItalic, R"(<strong><em>\1</em></strong>)");
        s.replace(reBold, R"(<strong>\1</strong>)");
        s.replace(reItalic, R"(<em>\1</em>)");
        s.replace(reStrike, R"(<del>\1</del>)");
        s.replace(reCode, R"(<code>\1</code>)");
        return s;
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        if (line.trimmed().startsWith("```")) {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeBlockLang = line.trimmed().mid(3).trimmed();
                codeContent.clear();
            } else {
                inCodeBlock = false;
                if (!codeBlockLang.isEmpty())
                    html += "<pre><code class=\"language-" + codeBlockLang + "\">" + escapeHtml(codeContent) + "</code></pre>\n";
                else
                    html += "<pre><code>" + escapeHtml(codeContent) + "</code></pre>\n";
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeContent.isEmpty()) codeContent += '\n';
            codeContent += line;
            continue;
        }

        QString trimmed = line.trimmed();

        // Table
        if (trimmed.contains('|') && trimmed.startsWith('|')) {
            QStringList cells;
            for (const auto &cell : trimmed.split('|', Qt::SkipEmptyParts))
                cells << cell.trimmed();
            bool isSep = true;
            for (const auto &c : cells) { QString t = c; t.remove('-'); t.remove(':'); if (!t.trimmed().isEmpty()) { isSep = false; break; } }
            if (!inTable) {
                inTable = true;
                html += "<table><thead><tr>";
                for (const auto &h : cells) html += "<th>" + processInline(h) + "</th>";
                html += "</tr></thead><tbody>\n";
            } else if (isSep) {
            } else {
                html += "<tr>";
                for (const auto &c : cells) html += "<td>" + processInline(c) + "</td>";
                html += "</tr>\n";
            }
            continue;
        } else if (inTable) { inTable = false; html += "</tbody></table>\n"; }

        if (trimmed.startsWith("> ")) {
            if (!inBlockquote) { inBlockquote = true; html += "<blockquote>\n"; }
            html += "<p>" + processInline(trimmed.mid(2)) + "</p>\n";
            continue;
        } else if (inBlockquote) { inBlockquote = false; html += "</blockquote>\n"; }

        static QRegularExpression ulRe(R"(^(\s*)[*\-+]\s+(.+))");
        auto ulMatch = ulRe.match(line);
        if (ulMatch.hasMatch()) {
            if (!inList) { inList = true; html += "<ul>\n"; }
            html += "<li>" + processInline(ulMatch.captured(2)) + "</li>\n";
            continue;
        } else if (inList && !inOrderedList) { inList = false; html += "</ul>\n"; }

        static QRegularExpression olRe(R"(^(\s*)\d+\.\s+(.+))");
        auto olMatch = olRe.match(line);
        if (olMatch.hasMatch()) {
            if (!inOrderedList) { inOrderedList = true; html += "<ol>\n"; }
            html += "<li>" + processInline(olMatch.captured(2)) + "</li>\n";
            continue;
        } else if (inOrderedList) { inOrderedList = false; html += "</ol>\n"; }

        if (trimmed.isEmpty()) { html += "\n"; continue; }
        static QRegularExpression reHr(R"(^[-*_]{3,}\s*$)");
        if (reHr.match(trimmed).hasMatch()) { html += "<hr>\n"; continue; }

        static QRegularExpression headingRe(R"(^(#{1,6})\s+(.+))");
        auto hMatch = headingRe.match(trimmed);
        if (hMatch.hasMatch()) {
            int level = hMatch.captured(1).length();
            html += QString("<h%1>%2</h%1>\n").arg(level).arg(processInline(hMatch.captured(2)));
            continue;
        }

        html += "<p>" + processInline(trimmed) + "</p>\n";
    }

    if (inCodeBlock) {
        if (!codeBlockLang.isEmpty())
            html += "<pre><code class=\"language-" + codeBlockLang + "\">" + escapeHtml(codeContent) + "</code></pre>\n";
        else
            html += "<pre><code>" + escapeHtml(codeContent) + "</code></pre>\n";
    }
    if (inList) html += "</ul>\n";
    if (inOrderedList) html += "</ol>\n";
    if (inBlockquote) html += "</blockquote>\n";
    if (inTable) html += "</tbody></table>\n";

    return html;
}
