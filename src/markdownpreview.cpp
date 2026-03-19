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
}

MarkdownPreview::~MarkdownPreview()
{
    if (m_cmarkLib) dlclose(m_cmarkLib);
}

void MarkdownPreview::setDarkMode(bool dark)
{
    m_dark = dark;
    // Re-render with new theme
    if (!m_pendingMd.isEmpty())
        render();
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

// loadBasePage removed — render() now generates full HTML page each time (like text-ed)

void MarkdownPreview::render()
{
    QString body = markdownToHtml(m_pendingMd);

    // Convert mermaid code blocks to <div class="mermaid">
    body.replace(QRegularExpression(
        R"(<pre><code class="language-mermaid">([\s\S]*?)</code></pre>)"),
        R"(<div class="mermaid">\1</div>)");

    // Build full HTML page (like text-ed: write to file + load via setUrl — avoids JS string escaping issues)
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
    QString hljsCss = m_katexDir + (m_dark ? "/hljs-dark.min.css" : "/hljs-light.min.css");

    QString page = QStringLiteral("<!DOCTYPE html>\n<html><head>\n<meta charset=\"utf-8\">\n"
        "<link rel=\"stylesheet\" href=\"file://") + m_katexDir + QStringLiteral("/katex.min.css\">\n"
        "<link rel=\"stylesheet\" href=\"file://") + hljsCss + QStringLiteral("\">\n"
        "<script src=\"file://") + m_katexDir + QStringLiteral("/katex.min.js\"></script>\n"
        "<script src=\"file://") + m_mermaidJsPath + QStringLiteral("\"></script>\n"
        "<script src=\"file://") + m_hljsPath + QStringLiteral("\"></script>\n"
        "<style>\n"
        "body { background: ") + bg + QStringLiteral("; color: ") + fg + QStringLiteral(";\n"
        "  font-family: -apple-system, 'Segoe UI', Helvetica, Arial, sans-serif;\n"
        "  font-size: 15px; line-height: 1.6; padding: 16px 24px; margin: 0; word-wrap: break-word; }\n"
        "h1,h2,h3,h4,h5,h6 { color: ") + headC + QStringLiteral("; margin-top: 24px; margin-bottom: 8px; font-weight: 600; }\n"
        "h1 { font-size: 2em; border-bottom: 1px solid ") + borderC + QStringLiteral("; padding-bottom: 0.3em; }\n"
        "h2 { font-size: 1.5em; border-bottom: 1px solid ") + borderC + QStringLiteral("; padding-bottom: 0.3em; }\n"
        "h3 { font-size: 1.25em; }\n"
        "p { margin: 8px 0; }\n"
        "a { color: ") + linkC + QStringLiteral("; text-decoration: none; }\n"
        "a:hover { text-decoration: underline; }\n"
        "code { background: ") + codeBg + QStringLiteral("; color: ") + codeFg + QStringLiteral(";\n"
        "  padding: 2px 6px; border-radius: 3px;\n"
        "  font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 0.9em; }\n"
        "pre { background: ") + codeBg + QStringLiteral("; padding: 12px 16px;\n"
        "  border-radius: 6px; overflow-x: auto; border: 1px solid ") + borderC + QStringLiteral("; }\n"
        "pre code { background: none; padding: 0; font-size: 0.85em; }\n"
        "pre code:not(.hljs) { color: ") + fg + QStringLiteral("; }\n"
        ".mermaid { text-align: center; }\n"
        "blockquote { border-left: 4px solid ") + bqBorder + QStringLiteral("; color: ") + bqFg + QStringLiteral("; margin: 8px 0; padding: 4px 16px; }\n"
        "ul, ol { padding-left: 24px; margin: 8px 0; }\n"
        "li { margin: 4px 0; }\n"
        "hr { border: none; border-top: 1px solid ") + hrC + QStringLiteral("; margin: 24px 0; }\n"
        "table { border-collapse: collapse; width: 100%; margin: 8px 0; }\n"
        "th, td { border: 1px solid ") + borderC + QStringLiteral("; padding: 6px 12px; text-align: left; }\n"
        "th { background: ") + thBg + QStringLiteral("; font-weight: 600; }\n"
        "img { max-width: 100%; border-radius: 4px; }\n"
        "del { text-decoration: line-through; opacity: 0.6; }\n"
        "@media print {\n"
        "  pre { white-space: pre-wrap !important; word-wrap: break-word !important;\n"
        "        overflow-x: visible !important; overflow-wrap: break-word !important; }\n"
        "  pre code { white-space: pre-wrap !important; word-wrap: break-word !important;\n"
        "             overflow-wrap: break-word !important; }\n"
        "  code { word-break: break-all !important; }\n"
        "  body { overflow-wrap: break-word; word-break: break-word; }\n"
        "  table { table-layout: fixed; }\n"
        "  td, th { word-wrap: break-word; overflow-wrap: break-word; }\n"
        "  img { max-width: 100% !important; }\n"
        "}\n"
        "</style>\n</head><body>\n")
        + body +
        QStringLiteral("\n<script>\n"
        "// KaTeX rendering\n"
        "if (typeof katex !== 'undefined') {\n"
        "    document.querySelectorAll('.katex-display').forEach(function(el) {\n"
        "        try { katex.render(el.textContent, el, { displayMode: true, throwOnError: false }); }\n"
        "        catch(e) { el.textContent = el.textContent; }\n"
        "    });\n"
        "    document.querySelectorAll('.katex-inline').forEach(function(el) {\n"
        "        try { katex.render(el.textContent, el, { displayMode: false, throwOnError: false }); }\n"
        "        catch(e) { el.textContent = el.textContent; }\n"
        "    });\n"
        "}\n"
        "// Code highlighting\n"
        "if (typeof hljs !== 'undefined') { hljs.highlightAll(); }\n"
        "// Mermaid\n"
        "if (typeof mermaid !== 'undefined') {\n"
        "    mermaid.initialize({ startOnLoad: false, theme: '") + mermaidTheme + QStringLiteral("',\n"
        "        suppressErrors: true, securityLevel: 'loose',\n"
        "        flowchart: { useMaxWidth: true, htmlLabels: true } });\n"
        "    mermaid.run().catch(function(e) {\n"
        "        document.querySelectorAll('.mermaid').forEach(function(el) {\n"
        "            if (el.getAttribute('data-processed')) return;\n"
        "            el.style.color = '#b91c1c'; el.style.fontStyle = 'italic'; el.style.whiteSpace = 'pre-wrap';\n"
        "        });\n"
        "    });\n"
        "}\n"
        "</script>\n</body></html>");

    QString tmpPath = QDir::tempPath() + "/vibe-coder-preview.html";
    QFile f(tmpPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(page.toUtf8());
        f.close();
    }
    m_webView->load(QUrl::fromLocalFile(tmpPath));
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
    // BUT: first strip out backtick code spans so $ inside code is not treated as math
    static QRegularExpression reCodeSpan(R"(`[^`]+`)");
    static QRegularExpression reCodeFence(R"(^```[\s\S]*?^```)", QRegularExpression::MultilineOption);
    static QRegularExpression reDisplayMath(R"(\$\$[\s\S]+?\$\$)");
    static QRegularExpression reInlineMath(R"(\$(?!\s)(?:[^$\\]|\\.)+?\$)");

    // Step 1: protect code spans and fenced code blocks from math detection
    QVector<QString> codeProtected;
    QString withCodeMasked = md;

    // Protect fenced code blocks first (multi-line), then inline code spans
    auto maskCode = [&](const QRegularExpression &re) {
        QRegularExpressionMatchIterator it = re.globalMatch(withCodeMasked);
        QVector<QRegularExpressionMatch> matches;
        while (it.hasNext()) matches.append(it.next());
        for (int i = matches.size() - 1; i >= 0; --i) {
            const auto &m = matches[i];
            QString placeholder = QString("\x05CODEBLK%1\x05").arg(codeProtected.size());
            codeProtected.append(m.captured());
            withCodeMasked.replace(m.capturedStart(), m.capturedLength(), placeholder);
        }
    };
    maskCode(reCodeFence);
    maskCode(reCodeSpan);

    // Step 2: protect math in code-masked text
    QVector<QString> mathFragments;
    QString protected_ = withCodeMasked;

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

    // Step 3: restore code blocks (they now have math removed from non-code areas)
    for (int i = 0; i < codeProtected.size(); ++i) {
        protected_.replace(QString("\x05CODEBLK%1\x05").arg(i), codeProtected[i]);
    }

    // Step 4: convert
    QString html;
    if (m_hasCmark) {
        html = cmarkConvert(protected_);
        // Restore math expressions
        for (int i = 0; i < mathFragments.size(); ++i) {
            html.replace(QString("\x02MATH%1\x02").arg(i), mathFragments[i].toHtmlEscaped());
        }
    } else {
        // Regex converter handles math internally via processInline (katex-display/katex-inline spans)
        html = regexConvert(md);
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

// Inline formatting — extract math placeholders, escape HTML, then apply inline patterns
// (Same approach as text-ed: placeholder-based math protection + toHtmlEscaped + inline regex)
static QString processInline(const QString &text)
{
    static const QRegularExpression reDispMath(QStringLiteral("\\$\\$(.+?)\\$\\$"));
    static const QRegularExpression reInlineMath(QStringLiteral("(?<!\\$)\\$(?!\\$)(.+?)(?<!\\$)\\$(?!\\$)"));
    static const QRegularExpression reBold(QStringLiteral("\\*\\*(.+?)\\*\\*"));
    static const QRegularExpression reItalic(QStringLiteral("(?<!\\*)\\*(?!\\*)(.+?)(?<!\\*)\\*(?!\\*)"));
    static const QRegularExpression reCode(QStringLiteral("`([^`]+)`"));
    static const QRegularExpression reStrike(QStringLiteral("~~(.+?)~~"));
    static const QRegularExpression reImg(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+)\\)"));
    static const QRegularExpression reLink(QStringLiteral("\\[([^\\]]+)\\]\\(([^)]+)\\)"));

    // Extract LaTeX placeholders before HTML-escaping (math content must not be escaped)
    struct Placeholder { qsizetype pos; qsizetype len; QString replacement; };
    QVector<Placeholder> placeholders;

    auto itD = reDispMath.globalMatch(text);
    while (itD.hasNext()) {
        auto m = itD.next();
        placeholders.append({m.capturedStart(), m.capturedLength(),
            QStringLiteral("<span class=\"katex-display\">") + m.captured(1) + QStringLiteral("</span>")});
    }

    auto itI = reInlineMath.globalMatch(text);
    while (itI.hasNext()) {
        auto m = itI.next();
        bool overlaps = false;
        for (const auto &p : placeholders) {
            if (m.capturedStart() >= p.pos && m.capturedStart() < p.pos + p.len) { overlaps = true; break; }
        }
        if (!overlaps) {
            placeholders.append({m.capturedStart(), m.capturedLength(),
                QStringLiteral("<span class=\"katex-inline\">") + m.captured(1) + QStringLiteral("</span>")});
        }
    }

    // Build result: escape non-math text, keep math raw
    QString result;
    if (placeholders.isEmpty()) {
        result = text.toHtmlEscaped();
    } else {
        std::sort(placeholders.begin(), placeholders.end(),
                  [](const Placeholder &a, const Placeholder &b) { return a.pos < b.pos; });
        qsizetype lastEnd = 0;
        for (const auto &p : placeholders) {
            result += text.mid(lastEnd, p.pos - lastEnd).toHtmlEscaped();
            result += p.replacement;
            lastEnd = p.pos + p.len;
        }
        result += text.mid(lastEnd).toHtmlEscaped();
    }

    // Extract inline code FIRST — protect content from bold/italic/link processing
    QVector<QString> codeFragments;
    {
        QRegularExpressionMatchIterator it = reCode.globalMatch(result);
        QVector<QRegularExpressionMatch> codeMatches;
        while (it.hasNext()) codeMatches.append(it.next());
        for (int ci = codeMatches.size() - 1; ci >= 0; --ci) {
            const auto &m = codeMatches[ci];
            QString placeholder = QStringLiteral("\x04CODE") + QString::number(codeFragments.size()) + QStringLiteral("\x04");
            codeFragments.append(QStringLiteral("<code>") + m.captured(1) + QStringLiteral("</code>"));
            result.replace(m.capturedStart(), m.capturedLength(), placeholder);
        }
    }

    // Apply inline formatting (code content is safely placeholder-protected)
    result.replace(reBold, QStringLiteral("<strong>\\1</strong>"));
    result.replace(reItalic, QStringLiteral("<em>\\1</em>"));
    result.replace(reStrike, QStringLiteral("<del>\\1</del>"));
    // Images MUST be before links (![alt](url) vs [text](url))
    result.replace(reImg, QStringLiteral("<img src=\"\\2\" alt=\"\\1\" style=\"max-width:100%\">"));
    result.replace(reLink, QStringLiteral("<a href=\"\\2\">\\1</a>"));

    // Restore inline code
    for (int ci = 0; ci < codeFragments.size(); ++ci) {
        result.replace(QStringLiteral("\x04CODE") + QString::number(ci) + QStringLiteral("\x04"), codeFragments[ci]);
    }

    return result;
}

QString MarkdownPreview::regexConvert(const QString &md)
{
    QString html;
    html.reserve(md.size());

    const QStringList lines = md.split('\n');
    bool inCodeBlock = false;
    bool inMermaid = false;
    QString codeContent;
    QString codeLang;
    bool inUl = false;
    bool inOl = false;
    bool inBlockquote = false;
    bool inTable = false;
    bool hadTableSep = false;
    QVector<QString> tableAligns;

    static const QRegularExpression reHead(QStringLiteral("^(#{1,6})\\s+(.*)"));
    static const QRegularExpression reHr(QStringLiteral("^(---|\\*\\*\\*|___)\\s*$"));
    static const QRegularExpression reUl(QStringLiteral("^\\s*[-*+]\\s+(.*)"));
    static const QRegularExpression reOl(QStringLiteral("^\\s*\\d+\\.\\s+(.*)"));

    auto closePending = [&]() {
        if (inUl) { html += QStringLiteral("</ul>\n"); inUl = false; }
        if (inOl) { html += QStringLiteral("</ol>\n"); inOl = false; }
        if (inBlockquote) { html += QStringLiteral("</blockquote>\n"); inBlockquote = false; }
        if (inTable) {
            html += (hadTableSep ? QStringLiteral("</tbody>") : QString()) + QStringLiteral("</table>\n");
            inTable = false; hadTableSep = false;
        }
    };

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        // ── Fenced code blocks ──
        if (trimmed.startsWith(QStringLiteral("```"))) {
            if (!inCodeBlock) {
                closePending();
                inCodeBlock = true;
                codeLang = trimmed.mid(3).trimmed();
                inMermaid = (codeLang.compare(QStringLiteral("mermaid"), Qt::CaseInsensitive) == 0);
                codeContent.clear();
            } else {
                if (inMermaid) {
                    html += QStringLiteral("<div class=\"mermaid\">\n") + codeContent + QStringLiteral("</div>\n");
                } else if (!codeLang.isEmpty()) {
                    html += QStringLiteral("<pre><code class=\"language-") + codeLang.toHtmlEscaped() +
                            QStringLiteral("\">") + codeContent.toHtmlEscaped() + QStringLiteral("</code></pre>\n");
                } else {
                    html += QStringLiteral("<pre><code>") + codeContent.toHtmlEscaped() + QStringLiteral("</code></pre>\n");
                }
                inCodeBlock = false;
                inMermaid = false;
            }
            continue;
        }
        if (inCodeBlock) {
            codeContent += line + '\n';
            continue;
        }

        // ── Blank line ──
        if (trimmed.isEmpty()) {
            closePending();
            html += QStringLiteral("<br>\n");
            continue;
        }

        // ── Headings ──
        auto mHead = reHead.match(trimmed);
        if (mHead.hasMatch()) {
            closePending();
            int level = mHead.captured(1).length();
            html += QStringLiteral("<h%1>%2</h%1>\n").arg(level).arg(processInline(mHead.captured(2)));
            continue;
        }

        // ── Horizontal rule ──
        if (reHr.match(trimmed).hasMatch()) {
            closePending();
            html += QStringLiteral("<hr>\n");
            continue;
        }

        // ── Table ──
        if (trimmed.startsWith('|') && trimmed.endsWith('|')) {
            QStringList cells;
            for (const auto &cell : trimmed.split('|', Qt::SkipEmptyParts))
                cells << cell.trimmed();

            bool isSep = !cells.isEmpty();
            for (const auto &c : cells) {
                QString t = c; t.remove('-'); t.remove(':'); t.remove(' ');
                if (!t.isEmpty()) { isSep = false; break; }
            }

            if (!inTable) {
                closePending();
                inTable = true; hadTableSep = false; tableAligns.clear();
                html += QStringLiteral("<table><thead><tr>");
                for (const auto &h : cells)
                    html += QStringLiteral("<th>") + processInline(h) + QStringLiteral("</th>");
                html += QStringLiteral("</tr></thead>\n");
            } else if (isSep && !hadTableSep) {
                hadTableSep = true; tableAligns.clear();
                for (const auto &s : cells) {
                    QString t = s.trimmed();
                    if (t.startsWith(':') && t.endsWith(':')) tableAligns << "center";
                    else if (t.endsWith(':')) tableAligns << "right";
                    else tableAligns << "left";
                }
                html += QStringLiteral("<tbody>\n");
            } else if (hadTableSep) {
                html += QStringLiteral("<tr>");
                for (int c = 0; c < cells.size(); ++c) {
                    QString align = (c < tableAligns.size()) ? tableAligns[c] : "left";
                    html += QStringLiteral("<td style=\"text-align:") + align + QStringLiteral("\">") +
                            processInline(cells[c]) + QStringLiteral("</td>");
                }
                html += QStringLiteral("</tr>\n");
            }
            continue;
        }
        if (inTable) {
            html += (hadTableSep ? QStringLiteral("</tbody>") : QString()) + QStringLiteral("</table>\n");
            inTable = false; hadTableSep = false;
        }

        // ── Blockquote ──
        if (trimmed.startsWith('>')) {
            QString content = trimmed.mid(1);
            if (content.startsWith(' ')) content = content.mid(1);
            if (!inBlockquote) {
                if (inUl) { html += QStringLiteral("</ul>\n"); inUl = false; }
                if (inOl) { html += QStringLiteral("</ol>\n"); inOl = false; }
                inBlockquote = true;
                html += QStringLiteral("<blockquote>\n");
            }
            html += QStringLiteral("<p>") + processInline(content) + QStringLiteral("</p>\n");
            continue;
        }
        if (inBlockquote) { html += QStringLiteral("</blockquote>\n"); inBlockquote = false; }

        // ── Unordered list ──
        auto mUl = reUl.match(line);
        if (mUl.hasMatch()) {
            if (inOl) { html += QStringLiteral("</ol>\n"); inOl = false; }
            if (!inUl) { inUl = true; html += QStringLiteral("<ul>\n"); }
            html += QStringLiteral("<li>") + processInline(mUl.captured(1)) + QStringLiteral("</li>\n");
            continue;
        }

        // ── Ordered list ──
        auto mOl = reOl.match(line);
        if (mOl.hasMatch()) {
            if (inUl) { html += QStringLiteral("</ul>\n"); inUl = false; }
            if (!inOl) { inOl = true; html += QStringLiteral("<ol>\n"); }
            html += QStringLiteral("<li>") + processInline(mOl.captured(1)) + QStringLiteral("</li>\n");
            continue;
        }

        // ── Regular paragraph ──
        closePending();
        html += QStringLiteral("<p>") + processInline(trimmed) + QStringLiteral("</p>\n");
    }

    // Close remaining open blocks
    if (inCodeBlock) {
        if (inMermaid)
            html += QStringLiteral("<div class=\"mermaid\">\n") + codeContent + QStringLiteral("</div>\n");
        else
            html += QStringLiteral("<pre><code>") + codeContent.toHtmlEscaped() + QStringLiteral("</code></pre>\n");
    }
    closePending();

    return html;
}
