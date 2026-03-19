#include "workspacesearch.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>

WorkspaceSearch::WorkspaceSearch(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Search bar
    auto *searchBar = new QHBoxLayout;
    searchBar->setSpacing(4);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search in workspace...");
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &WorkspaceSearch::doSearch);
    searchBar->addWidget(m_searchEdit, 1);

    m_caseCheck = new QCheckBox("Aa");
    m_caseCheck->setToolTip("Case sensitive");
    searchBar->addWidget(m_caseCheck);

    m_regexCheck = new QCheckBox(".*");
    m_regexCheck->setToolTip("Regular expression");
    searchBar->addWidget(m_regexCheck);

    m_searchBtn = new QPushButton("Search");
    connect(m_searchBtn, &QPushButton::clicked, this, &WorkspaceSearch::doSearch);
    searchBar->addWidget(m_searchBtn);

    mainLayout->addLayout(searchBar);

    m_statusLabel = new QLabel;
    mainLayout->addWidget(m_statusLabel);

    // Results + preview
    m_splitter = new QSplitter(Qt::Horizontal);

    m_resultList = new QListWidget;
    connect(m_resultList, &QListWidget::itemClicked, this, &WorkspaceSearch::onResultClicked);
    connect(m_resultList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_results.size())
            emit fileRequested(m_results[idx].filePath, m_results[idx].line);
    });
    m_splitter->addWidget(m_resultList);

    m_preview = new QPlainTextEdit;
    m_preview->setReadOnly(true);
    m_splitter->addWidget(m_preview);

    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter, 1);
}

void WorkspaceSearch::setProjectDir(const QString &dir)
{
    m_projectDir = dir;
}

void WorkspaceSearch::focusSearch()
{
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void WorkspaceSearch::setViewerFont(const QFont &font)
{
    m_resultList->setFont(font);
    m_preview->setFont(font);
    m_searchEdit->setFont(font);
}

void WorkspaceSearch::setViewerColors(const QColor &bg, const QColor &fg)
{
    QString ss = QString("background-color: %1; color: %2;").arg(bg.name(), fg.name());
    m_resultList->setStyleSheet(ss);
    m_preview->setStyleSheet(ss);
}

void WorkspaceSearch::doSearch()
{
    QString query = m_searchEdit->text();
    if (query.isEmpty() || m_projectDir.isEmpty()) return;

    if (m_grepProc) {
        m_grepProc->kill();
        m_grepProc->deleteLater();
    }

    m_results.clear();
    m_resultList->clear();
    m_preview->clear();
    m_statusLabel->setText("Searching...");

    m_grepProc = new QProcess(this);
    m_grepProc->setWorkingDirectory(m_projectDir);

    QStringList args;
    args << "-rn" << "--max-count=500";

    // Exclude common dirs
    args << "--exclude-dir=.git" << "--exclude-dir=node_modules"
         << "--exclude-dir=__pycache__" << "--exclude-dir=build"
         << "--exclude-dir=dist" << "--exclude-dir=.cache"
         << "--exclude-dir=target" << "--exclude-dir=.LLM";

    if (!m_caseCheck->isChecked())
        args << "-i";

    if (m_regexCheck->isChecked())
        args << "-E";
    else
        args << "-F";

    args << "--" << query << ".";

    connect(m_grepProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WorkspaceSearch::onSearchFinished);

    m_grepProc->start("grep", args);
}

void WorkspaceSearch::onSearchFinished(int exitCode, QProcess::ExitStatus)
{
    if (!m_grepProc) return;

    QString output = m_grepProc->readAllStandardOutput();
    m_grepProc->deleteLater();
    m_grepProc = nullptr;

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        // Format: ./path:line:text
        int firstColon = line.indexOf(':');
        if (firstColon < 0) continue;
        int secondColon = line.indexOf(':', firstColon + 1);
        if (secondColon < 0) continue;

        SearchResult r;
        QString relPath = line.left(firstColon);
        if (relPath.startsWith("./"))
            relPath = relPath.mid(2);
        r.filePath = m_projectDir + "/" + relPath;
        r.line = line.mid(firstColon + 1, secondColon - firstColon - 1).toInt();
        r.lineText = line.mid(secondColon + 1).trimmed();

        m_results.append(r);

        // Show in list
        QString display = QString("%1:%2  %3").arg(relPath).arg(r.line).arg(r.lineText);
        if (display.length() > 200)
            display = display.left(200) + "...";
        auto *item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, m_results.size() - 1);
        m_resultList->addItem(item);
    }

    if (m_results.isEmpty()) {
        m_statusLabel->setText(exitCode == 1 ? "No results found." : "Search completed.");
    } else {
        m_statusLabel->setText(QString("%1 results found.").arg(m_results.size()));
    }
}

void WorkspaceSearch::onResultClicked(QListWidgetItem *item)
{
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_results.size()) return;

    const auto &r = m_results[idx];

    // Load context around the line
    QFile file(r.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QStringList allLines;
    while (!in.atEnd())
        allLines.append(in.readLine());

    int start = qMax(0, r.line - 4);
    int end = qMin(allLines.size(), r.line + 3);

    QString preview;
    for (int i = start; i < end; ++i) {
        QString prefix = (i == r.line - 1) ? ">> " : "   ";
        preview += QString("%1%2: %3\n").arg(prefix).arg(i + 1, 4).arg(allLines[i]);
    }

    m_preview->setPlainText(preview);
}
