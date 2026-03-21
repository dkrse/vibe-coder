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
#include <cstdlib>
#include <cmark-gfm.h>
#include <cmark-gfm-core-extensions.h>

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    m_webView->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    m_webView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    m_webView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    m_webView->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    m_webView->settings()->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
    layout->addWidget(m_webView);

    QString tmpDir = QDir::tempPath() + "/vibe-coder-assets";
    QDir().mkpath(tmpDir + "/fonts");

    auto extractResource = [](const QString &resPath, const QString &outPath) {
        QFile res(resPath);
        if (!res.open(QIODevice::ReadOnly)) return;
        QByteArray data = res.readAll();
        if (QFile::exists(outPath)) {
            QFile existing(outPath);
            if (existing.open(QIODevice::ReadOnly)) {
                if (existing.readAll() == data) return;
            }
        }
        QFile out(outPath);
        if (out.open(QIODevice::WriteOnly))
            out.write(data);
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

    QDir fontsRes(":/katex/fonts");
    for (const auto &entry : fontsRes.entryList()) {
        extractResource(":/katex/fonts/" + entry, tmpDir + "/fonts/" + entry);
    }

    cmark_gfm_core_extensions_ensure_registered();

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(500);
    connect(m_debounce, &QTimer::timeout, this, &MarkdownPreview::render);
}

MarkdownPreview::~MarkdownPreview()
{
}

void MarkdownPreview::setDarkMode(bool dark)
{
    m_dark = dark;
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

void MarkdownPreview::render()
{
    QString body = markdownToHtml(m_pendingMd);

    // Convert mermaid code blocks to <div class="mermaid">
    body.replace(QRegularExpression(
        R"(<pre><code class="language-mermaid">([\s\S]*?)</code></pre>)"),
        R"(<div class="mermaid">\1</div>)");

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
        "if (typeof hljs !== 'undefined') { hljs.highlightAll(); }\n"
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
    QPdfDocument doc;
    QBuffer buf;
    buf.setData(pdfData);
    buf.open(QIODevice::ReadOnly);
    doc.load(&buf);

    int pageCount = doc.pageCount();
    if (pageCount <= 0) {
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly))
            f.write(pdfData);
        return;
    }

    const int dpi = 300;
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

        QRectF fullRect = layout.fullRect(QPageLayout::Point);
        QRectF paintRect = layout.paintRect(QPageLayout::Point);
        double pxPerPt = dpi / 72.0;
        QMarginsF margins = layout.margins(QPageLayout::Point);

        QRectF target(-margins.left() * pxPerPt, -margins.top() * pxPerPt,
                      fullRect.width() * pxPerPt, fullRect.height() * pxPerPt);
        painter.drawImage(target, img);

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
        double overlap = 3.0;
        painter.drawRect(QRectF(-ml, -mt, fw, mt + overlap));
        painter.drawRect(QRectF(-ml, ph - overlap, fw, mb + overlap));
        painter.drawRect(QRectF(-ml, -mt, ml + overlap, fh));
        painter.drawRect(QRectF(pw - overlap, -mt, mr + overlap, fh));

        if (pageBorder) {
            painter.setPen(QPen(QColor(180, 180, 180), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(0, 0, pw, ph));
        }

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
        m_webView->page()->printToPdf(
            [this, filePath, layout, pageNumbering, pageBorder](const QByteArray &pdfData) {
                postProcessPdf(pdfData, filePath, layout, pageNumbering, pageBorder);
                removePrintCss();
            }, layout);
    });
}

// ── Markdown → HTML via cmark-gfm ──────────────────────────────────

// Walk AST text nodes (skipping code) and inject math as raw HTML nodes
static void injectMathSpans(cmark_node *root)
{
    static QRegularExpression reDispMath(QStringLiteral("\\$\\$([\\s\\S]+?)\\$\\$"));
    static QRegularExpression reInlineMath(QStringLiteral("(?<!\\$)\\$(?!\\s)(?:[^$\\\\]|\\\\.)+?\\$(?!\\$)"));

    // Collect text nodes first (modifying tree during iteration is tricky)
    QVector<cmark_node *> textNodes;
    cmark_iter *iter = cmark_iter_new(root);
    cmark_event_type ev;
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        if (ev != CMARK_EVENT_ENTER) continue;
        cmark_node *node = cmark_iter_get_node(iter);
        if (cmark_node_get_type(node) != CMARK_NODE_TEXT) continue;
        // Skip text inside code spans or code blocks
        cmark_node *p = cmark_node_parent(node);
        if (p) {
            cmark_node_type pt = cmark_node_get_type(p);
            if (pt == CMARK_NODE_CODE || pt == CMARK_NODE_CODE_BLOCK) continue;
        }
        textNodes.append(node);
    }
    cmark_iter_free(iter);

    for (cmark_node *node : textNodes) {
        QString text = QString::fromUtf8(cmark_node_get_literal(node));

        // Find all math matches
        struct MathMatch { qsizetype pos; qsizetype len; QString html; };
        QVector<MathMatch> matches;

        auto itD = reDispMath.globalMatch(text);
        while (itD.hasNext()) {
            auto m = itD.next();
            QString inner = m.captured(1);
            matches.append({m.capturedStart(), m.capturedLength(),
                QStringLiteral("<span class=\"katex-display\">") + inner.toHtmlEscaped() + QStringLiteral("</span>")});
        }

        auto itI = reInlineMath.globalMatch(text);
        while (itI.hasNext()) {
            auto m = itI.next();
            bool overlaps = false;
            for (const auto &dm : matches) {
                if (m.capturedStart() >= dm.pos && m.capturedStart() < dm.pos + dm.len) {
                    overlaps = true; break;
                }
            }
            if (overlaps) continue;
            QString full = m.captured();
            QString inner = full.mid(1, full.length() - 2);
            matches.append({m.capturedStart(), m.capturedLength(),
                QStringLiteral("<span class=\"katex-inline\">") + inner.toHtmlEscaped() + QStringLiteral("</span>")});
        }

        if (matches.isEmpty()) continue;

        std::sort(matches.begin(), matches.end(),
                  [](const MathMatch &a, const MathMatch &b) { return a.pos < b.pos; });

        // Replace text node with sequence of text + html_inline nodes
        qsizetype lastEnd = 0;
        cmark_node *insertAfter = nullptr;

        for (const auto &mm : matches) {
            // Text before match
            if (mm.pos > lastEnd) {
                QString before = text.mid(lastEnd, mm.pos - lastEnd);
                cmark_node *tn = cmark_node_new(CMARK_NODE_TEXT);
                cmark_node_set_literal(tn, before.toUtf8().constData());
                if (!insertAfter)
                    cmark_node_insert_before(node, tn);
                else
                    cmark_node_insert_after(insertAfter, tn);
                insertAfter = tn;
            }
            // Math as raw HTML
            cmark_node *hn = cmark_node_new(CMARK_NODE_HTML_INLINE);
            cmark_node_set_literal(hn, mm.html.toUtf8().constData());
            if (!insertAfter)
                cmark_node_insert_before(node, hn);
            else
                cmark_node_insert_after(insertAfter, hn);
            insertAfter = hn;
            lastEnd = mm.pos + mm.len;
        }

        // Text after last match
        if (lastEnd < text.length()) {
            QString after = text.mid(lastEnd);
            cmark_node *tn = cmark_node_new(CMARK_NODE_TEXT);
            cmark_node_set_literal(tn, after.toUtf8().constData());
            if (!insertAfter)
                cmark_node_insert_before(node, tn);
            else
                cmark_node_insert_after(insertAfter, tn);
        }

        // Remove original text node
        cmark_node_free(node);
    }
}

QString MarkdownPreview::markdownToHtml(const QString &md)
{
    QByteArray utf8 = md.toUtf8();
    int options = CMARK_OPT_DEFAULT | CMARK_OPT_UNSAFE;

    cmark_parser *parser = cmark_parser_new(options);

    // Attach GFM extensions
    const char *extNames[] = {"table", "strikethrough", "autolink", "tagfilter", nullptr};
    for (int i = 0; extNames[i]; ++i) {
        cmark_syntax_extension *ext = cmark_find_syntax_extension(extNames[i]);
        if (ext) cmark_parser_attach_syntax_extension(parser, ext);
    }

    cmark_parser_feed(parser, utf8.constData(), utf8.size());
    cmark_node *doc = cmark_parser_finish(parser);

    // Walk AST: inject math spans in text nodes (code nodes untouched by design)
    injectMathSpans(doc);

    char *result = cmark_render_html(doc, options, cmark_parser_get_syntax_extensions(parser));
    QString html = QString::fromUtf8(result);
    free(result);

    cmark_node_free(doc);
    cmark_parser_free(parser);

    return html;
}
