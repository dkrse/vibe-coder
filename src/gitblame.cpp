#include "gitblame.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QTextBlock>
#include <QFileInfo>
#include <QScrollBar>
#include <QDateTime>

// ── BlameGutter ─────────────────────────────────────────────────────

BlameGutter::BlameGutter(BlameView *view)
    : QWidget(view), m_view(view) {}

QSize BlameGutter::sizeHint() const
{
    return QSize(m_view->gutterWidth(), 0);
}

void BlameGutter::paintEvent(QPaintEvent *event)
{
    m_view->gutterPaintEvent(event);
}

// ── BlameView ───────────────────────────────────────────────────────

BlameView::BlameView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_gutter = new BlameGutter(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) { updateGutterWidth(); });
    connect(this, &QPlainTextEdit::updateRequest, this, &BlameView::updateGutter);

    updateGutterWidth();
}

int BlameView::gutterWidth() const
{
    if (m_lines.isEmpty()) return 0;
    // "abcdef12 AuthorName____  2024-01-15 "
    return fontMetrics().horizontalAdvance(QString(38, '0')) + 10;
}

void BlameView::updateGutterWidth()
{
    setViewportMargins(gutterWidth(), 0, 0, 0);
}

void BlameView::updateGutter(const QRect &rect, int dy)
{
    if (dy)
        m_gutter->scroll(0, dy);
    else
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateGutterWidth();
}

void BlameView::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
}

void BlameView::setBlameData(const QVector<BlameLine> &lines)
{
    m_lines = lines;

    QString text;
    for (const auto &bl : lines)
        text += bl.text + "\n";
    if (!text.isEmpty() && text.endsWith('\n'))
        text.chop(1);
    setPlainText(text);

    updateGutterWidth();
    m_gutter->update();
}

void BlameView::setViewerColors(const QColor &bg, const QColor &fg)
{
    m_bgColor = bg;
    m_fgColor = fg;
    QPalette pal = palette();
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Text, fg);
    setPalette(pal);
    m_gutter->update();
}

void BlameView::gutterPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_gutter);

    QColor gutterBg = m_bgColor.lighter(115);
    painter.fillRect(event->rect(), gutterBg);

    QColor gutterFg = m_fgColor;
    gutterFg.setAlpha(180);

    // Alternating colors for different commits
    QColor altBg1 = m_bgColor.lighter(120);
    QColor altBg2 = m_bgColor.lighter(108);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    int gw = gutterWidth();
    int lineHeight = fontMetrics().height();

    QString prevHash;
    bool altColor = false;

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (blockNumber < m_lines.size()) {
                const auto &bl = m_lines[blockNumber];

                // Alternate bg per commit
                if (bl.hash != prevHash) {
                    altColor = !altColor;
                    prevHash = bl.hash;
                }
                painter.fillRect(0, top, gw, bottom - top, altColor ? altBg1 : altBg2);

                // Show annotation on first line of each commit group
                bool isFirstOfGroup = (blockNumber == 0) ||
                    (blockNumber > 0 && blockNumber - 1 < m_lines.size() &&
                     m_lines[blockNumber - 1].hash != bl.hash);

                if (isFirstOfGroup) {
                    QString annot = QString("%1  %2  %3")
                        .arg(bl.hash.left(8))
                        .arg(bl.author.left(16).leftJustified(16, ' '))
                        .arg(bl.date);
                    painter.setPen(gutterFg);
                    painter.drawText(4, top, gw - 8, lineHeight,
                                     Qt::AlignLeft | Qt::AlignVCenter, annot);
                } else {
                    // Dimmed hash only
                    QColor dimFg = gutterFg;
                    dimFg.setAlpha(60);
                    painter.setPen(dimFg);
                    painter.drawText(4, top, gw - 8, lineHeight,
                                     Qt::AlignLeft | Qt::AlignVCenter, bl.hash.left(8));
                }
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// ── GitBlame ────────────────────────────────────────────────────────

GitBlame::GitBlame(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *topBar = new QHBoxLayout;
    m_fileLabel = new QLabel("No file");
    topBar->addWidget(m_fileLabel, 1);

    m_refreshBtn = new QPushButton("Refresh");
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        if (!m_filePath.isEmpty())
            blameFile(m_workDir, m_filePath);
    });
    topBar->addWidget(m_refreshBtn);

    layout->addLayout(topBar);

    m_blameView = new BlameView;
    layout->addWidget(m_blameView, 1);
}

void GitBlame::blameFile(const QString &workDir, const QString &filePath)
{
    m_workDir = workDir;
    m_filePath = filePath;

    QFileInfo fi(filePath);
    m_fileLabel->setText("Blame: " + fi.fileName());

    if (m_proc) {
        m_proc->kill();
        m_proc->deleteLater();
    }

    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(workDir);

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitBlame::onBlameFinished);

    m_proc->start("git", {"blame", "--porcelain", filePath});
}

void GitBlame::onBlameFinished(int exitCode, QProcess::ExitStatus)
{
    if (!m_proc) return;

    QString output = m_proc->readAllStandardOutput();
    QString errOutput = m_proc->readAllStandardError();
    m_proc->deleteLater();
    m_proc = nullptr;

    if (exitCode != 0) {
        emit outputMessage("Git blame failed: " + errOutput.left(200), 2);
        return;
    }

    // Parse porcelain format
    QVector<BlameLine> lines;
    QStringList rawLines = output.split('\n');

    QString currentHash, currentAuthor, currentDate;
    int lineNum = 0;

    auto isHexHash = [](const QString &s) -> bool {
        if (s.length() != 40) return false;
        for (auto ch : s) {
            if (!ch.isDigit() && (ch < 'a' || ch > 'f') && (ch < 'A' || ch > 'F'))
                return false;
        }
        return true;
    };

    for (int i = 0; i < rawLines.size(); ++i) {
        const QString &line = rawLines[i];
        if (line.isEmpty()) continue;

        // Code line — starts with tab
        if (line.startsWith('\t')) {
            BlameLine bl;
            bl.hash = currentHash;
            bl.author = currentAuthor;
            bl.date = currentDate;
            bl.lineNum = lineNum;
            bl.text = line.mid(1);
            lines.append(bl);
            continue;
        }

        // Header line: 40-hex-hash <orig> <final> [count]
        int firstSpace = line.indexOf(' ');
        if (firstSpace == 40 && isHexHash(line.left(40))) {
            currentHash = line.left(40);
            QStringList nums = line.mid(41).split(' ');
            if (nums.size() >= 2)
                lineNum = nums[1].toInt();
            continue;
        }

        // Metadata lines
        if (line.startsWith("author ")) {
            currentAuthor = line.mid(7);
        } else if (line.startsWith("author-time ")) {
            qint64 timestamp = line.mid(12).toLongLong();
            QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
            currentDate = dt.toString("yyyy-MM-dd");
        }
    }

    m_blameView->setBlameData(lines);
}

void GitBlame::setViewerFont(const QFont &font)
{
    m_blameView->setFont(font);
}

void GitBlame::setViewerColors(const QColor &bg, const QColor &fg)
{
    m_blameView->setViewerColors(bg, fg);
}
