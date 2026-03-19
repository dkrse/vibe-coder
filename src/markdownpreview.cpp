#include "markdownpreview.h"

#include <QVBoxLayout>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QBuffer>
#include <QWebEngineSettings>
#include <QWebChannel>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPainter>
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
    // file:// access needed for loading mermaid.js, katex and preview HTML from temp dir
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    m_webView->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    m_webView->settings()->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    layout->addWidget(m_webView);

    // Extract bundled JS/CSS to temp dir
    QString tmpDir = QDir::tempPath() + "/vibe-coder-assets";
    QDir().mkpath(tmpDir + "/fonts");

    auto extractResource = [](const QString &resPath, const QString &outPath) {
        QFile res(resPath);
        if (!res.open(QIODevice::ReadOnly)) return;
        QByteArray data = res.readAll();

        if (QFile::exists(outPath)) {
            // Verify integrity: re-extract if on-disk copy differs from bundled resource
            QFile existing(outPath);
            if (existing.open(QIODevice::ReadOnly)) {
                if (existing.readAll() == data)
                    return; // matches, no need to re-extract
            }
            // Mismatch or read failure — overwrite with known-good copy
        }

        QFile out(outPath);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
        }
    };

    m_mermaidJsPath = tmpDir + "/mermaid.min.js";
    extractResource(":/js/mermaid.min.js", m_mermaidJsPath);

    m_hljsPath = tmpDir + "/highlight.min.js";
    extractResource(":/js/highlight.min.js", m_hljsPath);
    extractResource(":/hljs/hljs-dark.min.css", tmpDir + "/hljs-dark.min.css");
    extractResource(":/hljs/hljs-light.min.css", tmpDir + "/hljs-light.min.css");

    m_katexDir = tmpDir;
    extractResource(":/katex/katex.min.js", tmpDir + "/katex.min.js");
    extractResource(":/katex/auto-render.min.js", tmpDir + "/auto-render.min.js");

    // Extract katex CSS with patched font paths (fonts/ -> file:// absolute)
    QString katexCssPath = tmpDir + "/katex.min.css";
    if (!QFile::exists(katexCssPath)) {
        QFile res(":/katex/katex.min.css");
        if (res.open(QIODevice::ReadOnly)) {
            QString css = QString::fromUtf8(res.readAll());
            css.replace("fonts/", "file://" + tmpDir + "/fonts/");
            QFile out(katexCssPath);
            if (out.open(QIODevice::WriteOnly))
                out.write(css.toUtf8());
        }
    }

    // Extract KaTeX woff2 fonts
    QDir fontsRes(":/katex/fonts");
    for (const auto &entry : fontsRes.entryList()) {
        extractResource(":/katex/fonts/" + entry, tmpDir + "/fonts/" + entry);
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
        if (!m_hasCmark) {
            dlclose(m_cmarkLib);
            m_cmarkLib = nullptr;
        }
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

void MarkdownPreview::zoomIn()
{
    m_webView->setZoomFactor(qMin(m_webView->zoomFactor() + 0.1, 5.0));
}

void MarkdownPreview::zoomOut()
{
    m_webView->setZoomFactor(qMax(m_webView->zoomFactor() - 0.1, 0.25));
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
pre code { background: none; padding: 0; font-size: 0.85em; }
pre code:not(.hljs) { color: %2; }
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
<link rel="stylesheet" href="file://%14/katex.min.css">
<link rel="stylesheet" href="file://%15">
<script src="file://%14/katex.min.js"></script>
<script src="file://%14/auto-render.min.js"></script>
<script src="file://%12"></script>
<script src="file://%16"></script>
<script>
var mermaidTheme = '%13';
function renderKatex() {
    if (typeof renderMathInElement === 'function') {
        renderMathInElement(document.getElementById('content'), {
            delimiters: [
                {left: '$$', right: '$$', display: true},
                {left: '$', right: '$', display: false},
                {left: '\\\\(', right: '\\\\)', display: false},
                {left: '\\\\[', right: '\\\\]', display: true}
            ],
            ignoredTags: ['script', 'noscript', 'style', 'textarea', 'pre', 'code', 'tt'],
            throwOnError: false
        });
    }
}
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
    // Syntax highlighting
    if (typeof hljs !== 'undefined') {
        document.querySelectorAll('pre code').forEach(function(el) {
            hljs.highlightElement(el);
        });
    }
    // Render KaTeX math
    renderKatex();
}
</script>
</head><body>
<div id="content"></div>
</body></html>)")
        .arg(bg, fg, codeBg, codeFg, borderC, linkC, headC,
             bqBorder, bqFg, hrC, thBg,
             m_mermaidJsPath, mermaidTheme, m_katexDir,
             m_katexDir + (m_dark ? "/hljs-dark.min.css" : "/hljs-light.min.css"),
             m_hljsPath);

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

void MarkdownPreview::injectPrintCss(std::function<void()> then)
{
    QString js = R"(
        (function() {
            var old = document.getElementById('pdf-print-fix');
            if (old) old.remove();
            var style = document.createElement('style');
            style.id = 'pdf-print-fix';
            style.textContent = '@page { margin: 0; } @media print { html, body { border: none !important; outline: none !important; box-shadow: none !important; } pre { white-space: pre-wrap !important; word-wrap: break-word !important; overflow-x: visible !important; } pre code { white-space: pre-wrap !important; word-wrap: break-word !important; } table { table-layout: fixed; } td, th { word-wrap: break-word; overflow-wrap: break-word; } img { max-width: 100% !important; } body { overflow-wrap: break-word; word-break: break-word; } }';
            document.head.appendChild(style);
        })()
    )";
    m_webView->page()->runJavaScript(js, [then](const QVariant &) { then(); });
}

void MarkdownPreview::removePrintCss()
{
    m_webView->page()->runJavaScript(
        "var el = document.getElementById('pdf-print-fix'); if (el) el.remove();");
}

void MarkdownPreview::postProcessPdf(const QByteArray &pdfData, const QString &filePath,
                                      const QPageLayout &layout, const QString &pageNumbering, bool pageBorder)
{
    // Load the source PDF to count pages and get page sizes
    QPdfDocument doc;
    QBuffer buf;
    buf.setData(pdfData);
    buf.open(QIODevice::ReadOnly);
    doc.load(&buf);

    int pageCount = doc.pageCount();
    if (pageCount <= 0) {
        // Fallback: just save the raw PDF
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly))
            f.write(pdfData);
        return;
    }

    // Render each page at 300 DPI and create a new PDF with page numbers
    const int dpi = 300;
    QSizeF pageSizePt = layout.fullRect(QPageLayout::Point).size();

    QPdfWriter writer(filePath);
    writer.setPageLayout(layout);
    writer.setResolution(dpi);

    QPainter painter(&writer);

    for (int i = 0; i < pageCount; ++i) {
        if (i > 0) writer.newPage();

        QSizeF srcSize = doc.pagePointSize(i);
        QSize renderSize(static_cast<int>(srcSize.width() * dpi / 72.0),
                         static_cast<int>(srcSize.height() * dpi / 72.0));
        QImage img = doc.render(i, renderSize);

        // Draw the rendered page image into the full page area (including margins)
        QRectF fullRect = layout.fullRect(QPageLayout::Point);
        QRectF paintRect = layout.paintRect(QPageLayout::Point);
        // QPdfWriter paints relative to the paint rect (margins already accounted for)
        // So we need to offset the image to cover the full page including margins
        double scaleX = paintRect.width() / static_cast<double>(renderSize.width()) * dpi / 72.0;
        double scaleY = paintRect.height() / static_cast<double>(renderSize.height()) * dpi / 72.0;

        // Map from points to device coords
        double pxPerPt = dpi / 72.0;
        QMarginsF margins = layout.margins(QPageLayout::Point);

        // Draw the full-page image (offset by negative margin since painter starts at paint rect)
        QRectF target(-margins.left() * pxPerPt, -margins.top() * pxPerPt,
                      fullRect.width() * pxPerPt, fullRect.height() * pxPerPt);
        painter.drawImage(target, img);

        // The source PDF may contain visible edges from body background/padding.
        // Paint white rectangles over the margin areas to cover any artifacts.
        double pw = paintRect.width() * pxPerPt;
        double ph = paintRect.height() * pxPerPt;
        double ml = margins.left() * pxPerPt;
        double mt = margins.top() * pxPerPt;
        double mr = margins.right() * pxPerPt;
        double mb = margins.bottom() * pxPerPt;
        double fw = fullRect.width() * pxPerPt;
        double fh = fullRect.height() * pxPerPt;
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        // Overlap into content area by a few pixels to cover edge artifacts
        double overlap = 3.0;
        // top margin strip
        painter.drawRect(QRectF(-ml, -mt, fw, mt + overlap));
        // bottom margin strip
        painter.drawRect(QRectF(-ml, ph - overlap, fw, mb + overlap));
        // left margin strip
        painter.drawRect(QRectF(-ml, -mt, ml + overlap, fh));
        // right margin strip
        painter.drawRect(QRectF(pw - overlap, -mt, mr + overlap, fh));

        // Draw border around the paint rect if requested
        if (pageBorder) {
            painter.setPen(QPen(QColor(180, 180, 180), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(0, 0, pw, ph));
        }

        // Draw page number centered at the bottom of the paint rect
        QString numText;
        if (pageNumbering == "page/total")
            numText = QString("%1 / %2").arg(i + 1).arg(pageCount);
        else
            numText = QString::number(i + 1);

        QFont font("Helvetica", 9);
        painter.setFont(font);
        painter.setPen(QColor(136, 136, 136));
        QFontMetricsF fm(font, &writer);
        double textWidth = fm.horizontalAdvance(numText);
        double paintWidthDev = paintRect.width() * pxPerPt;
        double paintHeightDev = paintRect.height() * pxPerPt;
        double x = (paintWidthDev - textWidth) / 2.0;
        double y = paintHeightDev + margins.bottom() * pxPerPt * 0.4;
        painter.drawText(QPointF(x, y), numText);
    }

    painter.end();
}

void MarkdownPreview::exportToPdf(const QString &filePath, int marginLeft, int marginRight,
                                  const QString &pageNumbering, bool landscape, bool pageBorder)
{
    auto orientation = landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
    QPageLayout layout(QPageSize(QPageSize::A4), orientation,
                       QMarginsF(marginLeft, 10, marginRight, 10),
                       QPageLayout::Millimeter);

    bool needsPostProcess = (pageNumbering != "none") || pageBorder;

    injectPrintCss([this, filePath, layout, pageNumbering, pageBorder, needsPostProcess]() {
        if (!needsPostProcess) {
            m_webView->page()->printToPdf(filePath, layout);
            removePrintCss();
            return;
        }

        // Two-pass: first generate PDF to memory, then post-process with page numbers / border
        m_webView->page()->printToPdf(
            [this, filePath, layout, pageNumbering, pageBorder](const QByteArray &pdfData) {
                postProcessPdf(pdfData, filePath, layout, pageNumbering, pageBorder);
                removePrintCss();
            }, layout);
    });
}

// ── Markdown → HTML ─────────────────────────────────────────────────

QString MarkdownPreview::markdownToHtml(const QString &md)
{
    // Protect $$...$$ and $...$ from markdown processing (both cmark and regex)
    static QRegularExpression reDisplayMath(R"(\$\$[\s\S]+?\$\$)");
    static QRegularExpression reInlineMath(R"(\$(?!\s)(?:[^$\\]|\\.)+?\$)");

    QVector<QString> mathFragments;
    QString protected_ = md;

    // Protect display math first ($$...$$), then inline ($...$)
    auto protectMath = [&](const QRegularExpression &re) {
        QRegularExpressionMatchIterator it = re.globalMatch(protected_);
        QVector<QRegularExpressionMatch> matches;
        while (it.hasNext()) matches.append(it.next());
        for (int i = matches.size() - 1; i >= 0; --i) {
            const auto &m = matches[i];
            QString placeholder = QString("\x02MATH%1\x02").arg(mathFragments.size());
            mathFragments.append(m.captured());
            protected_.replace(m.capturedStart(), m.capturedLength(), placeholder);
        }
    };
    protectMath(reDisplayMath);
    protectMath(reInlineMath);

    QString html;
    if (m_hasCmark)
        html = cmarkConvert(protected_);
    else
        html = regexConvert(protected_);

    // Restore math expressions (HTML-escaped so KaTeX can read them from innerHTML)
    for (int i = 0; i < mathFragments.size(); ++i) {
        html.replace(QString("\x02MATH%1\x02").arg(i), mathFragments[i].toHtmlEscaped());
    }

    return html;
}

// Extract markdown tables and convert to HTML (cmark doesn't support GFM tables)
static QString extractAndConvertTables(const QString &md, QVector<QString> &tableHtmls)
{
    QStringList lines = md.split('\n');
    QString result;
    int i = 0;

    auto escapeHtml = [](const QString &s) {
        QString r = s;
        r.replace('&', "&amp;");
        r.replace('<', "&lt;");
        r.replace('>', "&gt;");
        return r;
    };

    while (i < lines.size()) {
        const QString &line = lines[i];
        QString trimmed = line.trimmed();

        // Detect table: line starts with | and next line is separator (|---|...)
        if (trimmed.startsWith('|') && trimmed.endsWith('|') && i + 1 < lines.size()) {
            QString nextTrimmed = lines[i + 1].trimmed();
            // Check if next line is separator: | --- | --- |
            bool isSep = nextTrimmed.startsWith('|') && nextTrimmed.contains('-');
            if (isSep) {
                QString sepClean = nextTrimmed;
                sepClean.remove('|'); sepClean.remove('-'); sepClean.remove(':'); sepClean.remove(' ');
                isSep = sepClean.isEmpty();
            }

            if (isSep) {
                // Parse table
                QString tableHtml = "<table><thead><tr>";
                // Header row
                QStringList headers = trimmed.split('|', Qt::SkipEmptyParts);
                for (const auto &h : headers)
                    tableHtml += "<th>" + escapeHtml(h.trimmed()) + "</th>";
                tableHtml += "</tr></thead><tbody>\n";

                // Parse alignment from separator
                QStringList sepCells = lines[i + 1].trimmed().split('|', Qt::SkipEmptyParts);
                QVector<QString> aligns;
                for (const auto &s : sepCells) {
                    QString t = s.trimmed();
                    if (t.startsWith(':') && t.endsWith(':')) aligns << "center";
                    else if (t.endsWith(':')) aligns << "right";
                    else aligns << "left";
                }

                i += 2; // skip header + separator

                // Body rows
                while (i < lines.size()) {
                    QString rowTrimmed = lines[i].trimmed();
                    if (!rowTrimmed.startsWith('|')) break;
                    QStringList cells = rowTrimmed.split('|', Qt::SkipEmptyParts);
                    tableHtml += "<tr>";
                    for (int c = 0; c < cells.size(); ++c) {
                        QString align = (c < aligns.size()) ? aligns[c] : "left";
                        tableHtml += "<td style=\"text-align:" + align + "\">" + escapeHtml(cells[c].trimmed()) + "</td>";
                    }
                    tableHtml += "</tr>\n";
                    ++i;
                }
                tableHtml += "</tbody></table>\n";

                // Store and insert placeholder
                QString placeholder = QString("\x03TABLE%1\x03").arg(tableHtmls.size());
                tableHtmls.append(tableHtml);
                result += placeholder + "\n";
                continue;
            }
        }

        result += line + "\n";
        ++i;
    }
    return result;
}

QString MarkdownPreview::cmarkConvert(const QString &md)
{
    // Pre-extract tables (cmark doesn't support GFM tables)
    QVector<QString> tableHtmls;
    QString preprocessed = extractAndConvertTables(md, tableHtmls);

    QByteArray utf8 = preprocessed.toUtf8();
    char *result = m_cmarkToHtml(utf8.constData(), utf8.size(), (1 << 17));
    if (!result) return regexConvert(md);
    QString html = QString::fromUtf8(result);
    free(result);

    // Restore table HTML (cmark may have wrapped placeholders in <p> tags)
    for (int i = 0; i < tableHtmls.size(); ++i) {
        QString placeholder = QString("\x03TABLE%1\x03").arg(i);
        // Remove <p> wrapper if cmark added one
        html.replace("<p>" + placeholder + "</p>", tableHtmls[i]);
        html.replace(placeholder, tableHtmls[i]);
    }

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
