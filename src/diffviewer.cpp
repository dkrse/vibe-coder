#include "diffviewer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QFont>

// ── DiffHighlighter ─────────────────────────────────────────────────

DiffHighlighter::DiffHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {}

void DiffHighlighter::highlightBlock(const QString &text)
{
    QTextCharFormat fmt;

    if (text.startsWith("+++") || text.startsWith("---")) {
        fmt.setForeground(QColor("#569cd6"));
        fmt.setFontWeight(QFont::Bold);
        setFormat(0, text.length(), fmt);
    } else if (text.startsWith("@@")) {
        fmt.setForeground(QColor("#c586c0"));
        setFormat(0, text.length(), fmt);
    } else if (text.startsWith("+")) {
        fmt.setForeground(QColor("#4ec9b0"));
        fmt.setBackground(QColor("#1e3a1e"));
        setFormat(0, text.length(), fmt);
    } else if (text.startsWith("-")) {
        fmt.setForeground(QColor("#f44747"));
        fmt.setBackground(QColor("#3a1e1e"));
        setFormat(0, text.length(), fmt);
    } else if (text.startsWith("diff ")) {
        fmt.setForeground(QColor("#dcdcaa"));
        fmt.setFontWeight(QFont::Bold);
        setFormat(0, text.length(), fmt);
    }
}

// ── DiffViewer ──────────────────────────────────────────────────────

DiffViewer::DiffViewer(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *toolbar = new QHBoxLayout;

    m_modeCombo = new QComboBox;
    m_modeCombo->addItems({"Working Changes", "Staged Changes", "Last Commit"});
    toolbar->addWidget(m_modeCombo);

    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setFixedWidth(80);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_textEdit = new QPlainTextEdit;
    m_textEdit->setReadOnly(true);
    m_textEdit->setFont(QFont("Monospace", 10));
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Colors handled by global theme stylesheet

    m_highlighter = new DiffHighlighter(m_textEdit->document());

    layout->addWidget(m_textEdit, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        if (!m_workDir.isEmpty())
            refresh(m_workDir);
    });

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        if (!m_workDir.isEmpty())
            refresh(m_workDir);
    });
}

void DiffViewer::refresh(const QString &workDir)
{
    m_workDir = workDir;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(workDir);

    QStringList args;
    int mode = m_modeCombo->currentIndex();
    if (mode == 0) {
        args = {"diff", "--no-color"};
    } else if (mode == 1) {
        args = {"diff", "--cached", "--no-color"};
    } else {
        args = {"diff", "HEAD~1", "--no-color"};
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        QString output = QString::fromUtf8(proc->readAllStandardOutput());
        static constexpr int MAX_DIFF_SIZE = 512 * 1024; // 512KB
        if (output.size() > MAX_DIFF_SIZE) {
            output.truncate(MAX_DIFF_SIZE);
            output += "\n\n--- Output truncated (>512KB) ---";
        }
        if (output.trimmed().isEmpty())
            output = "(no changes)";
        setDiffText(output);
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() {
        proc->deleteLater();
    });

    proc->start("git", args);
}

void DiffViewer::setDiffText(const QString &text)
{
    m_textEdit->setPlainText(text);
}

void DiffViewer::setViewerFont(const QFont &font)
{
    m_textEdit->setFont(font);
}

void DiffViewer::setViewerColors(const QColor &, const QColor &)
{
    // Colors are now handled by the global theme stylesheet
}
