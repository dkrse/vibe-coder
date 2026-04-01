#include "gitgraph.h"
#include "sshmanager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QProcess>
#include <QDialog>
#include <QListWidget>
#include <QRegularExpression>
#include <memory>
#include <QLineEdit>
#include "themeddialog.h"

// ── Colors for graph lanes ─────────────────────────────────────────

static const QColor s_laneColors[] = {
    QColor("#61afef"), // blue
    QColor("#e06c75"), // red
    QColor("#98c379"), // green
    QColor("#e5c07b"), // yellow
    QColor("#c678dd"), // purple
    QColor("#56b6c2"), // cyan
    QColor("#d19a66"), // orange
    QColor("#be5046"), // dark red
};
static constexpr int NUM_LANE_COLORS = sizeof(s_laneColors) / sizeof(s_laneColors[0]);

// ── GitGraphView ───────────────────────────────────────────────────

GitGraphView::GitGraphView(QWidget *parent)
    : QWidget(parent)
{
    m_font = QFont("Monospace", 11);
    setMinimumWidth(400);
}

QColor GitGraphView::laneColor(int col) const
{
    return s_laneColors[col % NUM_LANE_COLORS];
}

void GitGraphView::setCommits(const QVector<GitCommitInfo> &commits)
{
    m_commits = commits;
    computeGraph();
    setMinimumHeight(static_cast<int>(m_commits.size()) * ROW_HEIGHT + ROW_HEIGHT);
    update();
}

void GitGraphView::setColors(const QColor &bg, const QColor &fg)
{
    m_bgColor = bg;
    m_fgColor = fg;
    update();
}

void GitGraphView::setGraphFont(const QFont &font)
{
    m_font = font;
    update();
}

void GitGraphView::computeGraph()
{
    m_graphRows.clear();
    m_maxColumns = 1;

    if (m_commits.isEmpty()) return;

    // Lanes track active branch lines
    QVector<GraphLane> lanes;
    m_graphRows.resize(m_commits.size());

    for (int i = 0; i < m_commits.size(); ++i) {
        auto &commit = m_commits[i];
        auto &row = m_graphRows[i];

        // Find which lane this commit belongs to
        int myLane = -1;
        for (int l = 0; l < lanes.size(); ++l) {
            if (lanes[l].active && lanes[l].hash == commit.hash) {
                myLane = l;
                break;
            }
        }

        // If not found in any lane, add a new lane
        if (myLane < 0) {
            myLane = -1;
            for (int l = 0; l < lanes.size(); ++l) {
                if (!lanes[l].active) { myLane = l; break; }
            }
            if (myLane < 0) {
                myLane = lanes.size();
                lanes.append({commit.hash, true});
            } else {
                lanes[myLane] = {commit.hash, true};
            }
        }

        commit.column = myLane;
        row.column = myLane;

        // Process parents
        if (commit.parents.isEmpty()) {
            lanes[myLane].active = false;
        } else {
            lanes[myLane].hash = commit.parents[0];
            row.lines.append({myLane, myLane});

            for (int p = 1; p < commit.parents.size(); ++p) {
                const QString &parentHash = commit.parents[p];

                int parentLane = -1;
                for (int l = 0; l < lanes.size(); ++l) {
                    if (lanes[l].active && lanes[l].hash == parentHash) {
                        parentLane = l;
                        break;
                    }
                }

                if (parentLane >= 0) {
                    row.lines.append({myLane, parentLane});
                } else {
                    int newLane = -1;
                    for (int l = 0; l < lanes.size(); ++l) {
                        if (!lanes[l].active) { newLane = l; break; }
                    }
                    if (newLane < 0) {
                        newLane = lanes.size();
                        lanes.append({parentHash, true});
                    } else {
                        lanes[newLane] = {parentHash, true};
                    }
                    row.lines.append({myLane, newLane});
                }
            }
        }

        for (int l = 0; l < lanes.size(); ++l) {
            if (l != myLane && lanes[l].active) {
                row.lines.append({l, l});
            }
        }

        if (lanes.size() > m_maxColumns)
            m_maxColumns = lanes.size();
    }
}

QSize GitGraphView::sizeHint() const
{
    return {800, static_cast<int>(m_commits.size()) * ROW_HEIGHT + ROW_HEIGHT};
}

QSize GitGraphView::minimumSizeHint() const
{
    return {400, static_cast<int>(m_commits.size()) * ROW_HEIGHT + ROW_HEIGHT};
}

void GitGraphView::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(event->rect(), m_bgColor);

    if (m_commits.isEmpty()) {
        p.setPen(m_fgColor);
        p.setFont(m_font);
        p.drawText(rect(), Qt::AlignCenter, "No commits found");
        return;
    }

    int graphWidth = LEFT_MARGIN + m_maxColumns * COL_WIDTH + TEXT_OFFSET;

    QFont textFont = m_font;
    QFont smallFont = m_font;
    smallFont.setPointSize(m_font.pointSize() - 1);
    QFont boldFont = m_font;
    boldFont.setBold(true);
    boldFont.setPointSize(m_font.pointSize() - 1);
    QFontMetrics fm(textFont);

    for (int i = 0; i < m_commits.size(); ++i) {
        int y = i * ROW_HEIGHT + ROW_HEIGHT / 2;
        const auto &commit = m_commits[i];
        const auto &row = m_graphRows[i];

        // Draw lane lines
        for (const auto &line : row.lines) {
            int fromX = LEFT_MARGIN + line.first * COL_WIDTH + COL_WIDTH / 2;
            int toX = LEFT_MARGIN + line.second * COL_WIDTH + COL_WIDTH / 2;
            int lineCol = qMin(line.first, line.second);

            QPen linePen(laneColor(lineCol), 2);
            // Remote-only commits get dashed lines
            if (commit.isRemoteOnly)
                linePen.setStyle(Qt::DashLine);
            p.setPen(linePen);

            if (fromX == toX) {
                p.drawLine(fromX, y - ROW_HEIGHT / 2, fromX, y + ROW_HEIGHT / 2);
            } else {
                QPainterPath path;
                path.moveTo(fromX, y);
                path.cubicTo(fromX, y + ROW_HEIGHT / 2,
                             toX, y,
                             toX, y + ROW_HEIGHT / 2);
                p.drawPath(path);
            }
        }

        // Draw commit node
        int nodeX = LEFT_MARGIN + row.column * COL_WIDTH + COL_WIDTH / 2;
        QColor nodeColor = laneColor(row.column);

        if (commit.isRemoteOnly) {
            // Remote-only: diamond shape
            p.setPen(QPen(nodeColor, 2));
            p.setBrush(m_bgColor);
            QPolygonF diamond;
            diamond << QPointF(nodeX, y - NODE_RADIUS)
                    << QPointF(nodeX + NODE_RADIUS, y)
                    << QPointF(nodeX, y + NODE_RADIUS)
                    << QPointF(nodeX - NODE_RADIUS, y);
            p.drawPolygon(diamond);
        } else if (commit.parents.size() > 1) {
            // Merge commit: hollow circle
            p.setPen(QPen(nodeColor, 2));
            p.setBrush(m_bgColor);
            p.drawEllipse(QPointF(nodeX, y), NODE_RADIUS, NODE_RADIUS);
        } else {
            // Normal commit: filled circle
            p.setPen(Qt::NoPen);
            p.setBrush(nodeColor);
            p.drawEllipse(QPointF(nodeX, y), NODE_RADIUS, NODE_RADIUS);
        }

        // Draw text: hash + refs + subject
        int textX = graphWidth;

        // Short hash
        p.setFont(smallFont);
        p.setPen(commit.isRemoteOnly ? QColor("#666666") : QColor("#888888"));
        p.drawText(textX, y + fm.ascent() / 2 - 1, commit.shortHash);
        textX += fm.horizontalAdvance("00000000");

        // Refs (branch/tag labels)
        if (!commit.refs.isEmpty()) {
            for (const auto &ref : commit.refs) {
                QColor refBg, refFg;
                bool isRemoteRef = ref.startsWith("origin/") || ref.contains("/");

                if (ref.startsWith("tag:")) {
                    refBg = QColor("#e5c07b");
                    refFg = QColor("#1e1e1e");
                } else if (ref == "HEAD") {
                    refBg = QColor("#61afef");
                    refFg = QColor("#1e1e1e");
                } else if (isRemoteRef) {
                    // Remote branches: distinct color (orange-ish)
                    refBg = QColor("#d19a66");
                    refFg = QColor("#1e1e1e");
                } else {
                    // Local branches: green
                    refBg = QColor("#98c379");
                    refFg = QColor("#1e1e1e");
                }

                QString label = ref;
                p.setFont(boldFont);
                QFontMetrics refFm(boldFont);
                int labelW = refFm.horizontalAdvance(label) + 8;
                int labelH = refFm.height() + 2;
                int labelY = y - labelH / 2;

                p.setPen(Qt::NoPen);
                p.setBrush(refBg);
                p.drawRoundedRect(textX, labelY, labelW, labelH, 3, 3);
                p.setPen(refFg);
                p.drawText(textX + 4, y + refFm.ascent() / 2 - 1, label);
                textX += labelW + 4;
            }
        }

        // Subject
        p.setFont(textFont);
        p.setPen(commit.isRemoteOnly ? QColor("#777777") : m_fgColor);
        QString subject = commit.subject;
        int availWidth = width() - textX - 180;
        if (fm.horizontalAdvance(subject) > availWidth)
            subject = fm.elidedText(subject, Qt::ElideRight, availWidth);
        p.drawText(textX + 4, y + fm.ascent() / 2 - 1, subject);

        // Author + date on the right
        p.setFont(smallFont);
        p.setPen(QColor("#888888"));
        QString meta = commit.author + "  " + commit.date;
        QFontMetrics sfm(smallFont);
        int metaW = sfm.horizontalAdvance(meta);
        p.drawText(width() - metaW - 12, y + sfm.ascent() / 2 - 1, meta);
    }
}

// ── GitGraph (container widget) ────────────────────────────────────

GitGraph::GitGraph(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 4, 4, 4);
    toolbar->setSpacing(4);

    m_branchCombo = new QComboBox;
    m_branchCombo->setMinimumWidth(150);
    m_branchCombo->addItem("--all--");
    toolbar->addWidget(m_branchCombo);

    m_refreshBtn = new QPushButton("Refresh");
    toolbar->addWidget(m_refreshBtn);

    // Separator
    auto *sep1 = new QWidget;
    sep1->setFixedWidth(8);
    toolbar->addWidget(sep1);

    m_fetchBtn = new QPushButton("Fetch");
    m_fetchBtn->setToolTip("Fetch from remote (git fetch --all --prune)");
    toolbar->addWidget(m_fetchBtn);

    m_pullBtn = new QPushButton("Pull");
    m_pullBtn->setToolTip("Pull current branch from remote (git pull)");
    toolbar->addWidget(m_pullBtn);

    m_pushBtn = new QPushButton("Push");
    m_pushBtn->setToolTip("Push current branch to remote (git push)");
    toolbar->addWidget(m_pushBtn);

    // Separator
    auto *sep2 = new QWidget;
    sep2->setFixedWidth(8);
    toolbar->addWidget(sep2);

    m_commitBtn = new QPushButton("Commit");
    m_commitBtn->setToolTip("Stage all changes and commit (git add . && git commit)");
    toolbar->addWidget(m_commitBtn);

    // Separator
    auto *sep3 = new QWidget;
    sep3->setFixedWidth(8);
    toolbar->addWidget(sep3);

    // Tracking info label
    m_trackingLabel = new QLabel;
    m_trackingLabel->setStyleSheet("color: #888888; padding: 0 4px;");
    toolbar->addWidget(m_trackingLabel);

    toolbar->addStretch();

    // User info button
    m_userBtn = new QPushButton("User");
    m_userBtn->setToolTip("Git user name and email");
    toolbar->addWidget(m_userBtn);

    connect(m_userBtn, &QPushButton::clicked, this, &GitGraph::showUserDialog);

    // Remotes button
    m_remoteBtn = new QPushButton("Remotes");
    m_remoteBtn->setToolTip("View and manage remote repositories");
    toolbar->addWidget(m_remoteBtn);

    connect(m_remoteBtn, &QPushButton::clicked, this, &GitGraph::showRemotesDialog);
    layout->addLayout(toolbar);

    // Scroll area with graph
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_graphView = new GitGraphView;
    m_scrollArea->setWidget(m_graphView);
    layout->addWidget(m_scrollArea, 1);

    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        refresh(m_workDir);
    });

    connect(m_branchCombo, &QComboBox::currentTextChanged, this, [this]() {
        refresh(m_workDir);
    });

    connect(m_fetchBtn, &QPushButton::clicked, this, [this]() {
        m_fetchBtn->setEnabled(false);
        m_fetchBtn->setText("Fetching...");
        runGitCommand({"fetch", "--all", "--prune"}, "Fetch complete", "Fetch failed");
    });

    connect(m_pullBtn, &QPushButton::clicked, this, [this]() {
        m_pullBtn->setEnabled(false);
        m_pullBtn->setText("Pulling...");
        runGitCommand({"pull"}, "Pull complete", "Pull failed");
    });

    connect(m_commitBtn, &QPushButton::clicked, this, [this]() {
        emit commitRequested();
    });

    connect(m_pushBtn, &QPushButton::clicked, this, [this]() {
        m_pushBtn->setEnabled(false);
        m_pushBtn->setText("Pushing...");
        runGitCommand({"push"}, "Push complete", "Push failed");
    });
}

void GitGraph::refresh(const QString &workDir)
{
    m_workDir = workDir;
    if (m_workDir.isEmpty()) return;

    // Single combined git command instead of 5 separate processes
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode != 0) {
            proc->deleteLater();
            return;
        }
        QString output = QString::fromUtf8(proc->readAllStandardOutput());
        proc->deleteLater();

        // Parse sections
        int branchIdx = output.indexOf("%%BRANCHES%%");
        int trackIdx = output.indexOf("%%TRACKING%%");
        int remoteIdx = output.indexOf("%%REMOTES%%");
        int userIdx = output.indexOf("%%USER%%");
        int localIdx = output.indexOf("%%LOCAL_HASHES%%");
        int logIdx = output.indexOf("%%LOG%%");

        if (branchIdx < 0) return;

        // Parse branches
        QString branchOutput = output.mid(branchIdx + 12, trackIdx - branchIdx - 12).trimmed();
        {
            QString current = m_branchCombo->currentText();
            m_branchCombo->blockSignals(true);
            m_branchCombo->clear();
            m_branchCombo->addItem("--all--");
            QStringList localBranches, remoteBranches;
            for (const auto &line : branchOutput.split('\n', Qt::SkipEmptyParts)) {
                QString branch = line.trimmed();
                if (branch.startsWith("* ")) branch = branch.mid(2);
                if (branch.isEmpty() || branch.startsWith("(")) continue;
                if (branch.startsWith("remotes/")) {
                    QString remote = branch.mid(8);
                    if (!remote.contains("/HEAD")) remoteBranches.append(remote);
                } else {
                    localBranches.append(branch);
                }
            }
            for (const auto &b : localBranches) m_branchCombo->addItem(b);
            if (!remoteBranches.isEmpty()) {
                m_branchCombo->insertSeparator(m_branchCombo->count());
                for (const auto &b : remoteBranches) m_branchCombo->addItem(b);
            }
            int idx = m_branchCombo->findText(current);
            m_branchCombo->setCurrentIndex(idx >= 0 ? idx : 0);
            m_branchCombo->blockSignals(false);
        }

        // Parse tracking
        QString trackOutput = output.mid(trackIdx + 12, remoteIdx - trackIdx - 12).trimmed();
        parseTrackingInfo(trackOutput);

        // Parse remotes
        QString remoteOutput = output.mid(remoteIdx + 11, userIdx - remoteIdx - 11).trimmed();
        {
            m_remotes.clear();
            for (const auto &line : remoteOutput.split('\n', Qt::SkipEmptyParts)) {
                if (!line.contains("(fetch)")) continue;
                int tabI = line.indexOf('\t');
                if (tabI < 0) continue;
                QString name = line.left(tabI).trimmed();
                QString url = line.mid(tabI + 1).trimmed();
                url.remove(QRegularExpression("\\s*\\(fetch\\)\\s*$"));
                m_remotes[name] = url;
            }
            if (m_remotes.isEmpty()) {
                m_remoteBtn->setText("Remotes");
            } else {
                m_remoteBtn->setText(QString("Remotes (%1)").arg(m_remotes.size()));
                QStringList tip;
                for (auto it = m_remotes.begin(); it != m_remotes.end(); ++it)
                    tip << it.key() + ": " + it.value();
                m_remoteBtn->setToolTip(tip.join('\n'));
            }
        }

        // Parse user
        QString userOutput = output.mid(userIdx + 8, localIdx - userIdx - 8).trimmed();
        {
            QStringList userLines = userOutput.split('\n', Qt::SkipEmptyParts);
            QString userName = userLines.value(0).trimmed();
            QString userEmail = userLines.value(1).trimmed();
            if (!userName.isEmpty() || !userEmail.isEmpty())
                m_userBtn->setText(userName);
        }

        // Parse local hashes + log, compute graph
        QString localHashOutput = output.mid(localIdx + 16, logIdx - localIdx - 16).trimmed();
        QString logOutput = output.mid(logIdx + 7).trimmed();

        QSet<QString> localHashes;
        for (const auto &line : localHashOutput.split('\n', Qt::SkipEmptyParts))
            localHashes.insert(line.trimmed());

        auto commits = parseGitLog(logOutput);

        // Mark remote-only commits using hash-map lookup instead of O(n²) walk
        QHash<QString, int> hashToIdx;
        hashToIdx.reserve(commits.size());
        for (int i = 0; i < commits.size(); ++i)
            hashToIdx[commits[i].hash] = i;

        QVector<bool> reachable(commits.size(), false);
        QVector<int> stack;
        for (int i = 0; i < commits.size(); ++i) {
            if (localHashes.contains(commits[i].hash)) {
                reachable[i] = true;
                stack.append(i);
            }
        }
        while (!stack.isEmpty()) {
            int ci = stack.takeLast();
            for (const auto &p : commits[ci].parents) {
                auto it = hashToIdx.find(p);
                if (it != hashToIdx.end() && !reachable[it.value()]) {
                    reachable[it.value()] = true;
                    stack.append(it.value());
                }
            }
        }
        for (int i = 0; i < commits.size(); ++i) {
            if (!reachable[i]) commits[i].isRemoteOnly = true;
        }

        m_graphView->setCommits(commits);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() {
        proc->deleteLater();
    });

    // Determine which branch to log
    QString branch = m_branchCombo->currentText();
    QString logBranch = (branch.isEmpty() || branch == "--all--") ? "--all" : branch;

    QString cmd =
        "echo '%%BRANCHES%%' && git branch -a 2>/dev/null; "
        "echo '%%TRACKING%%' && git status -sb --porcelain=v1 2>/dev/null; "
        "echo '%%REMOTES%%' && git remote -v 2>/dev/null; "
        "echo '%%USER%%' && git config user.name 2>/dev/null; git config user.email 2>/dev/null; "
        "echo '%%LOCAL_HASHES%%' && git rev-list --branches -n 200 2>/dev/null; "
        "echo '%%LOG%%' && git --no-optional-locks log --format='%H|%h|%P|%D|%an|%cr|%s' -n 200 " + logBranch;

    if (isSshActive()) {
        // Run git commands on remote server via SSH (not on slow sshfs mount)
        QString remotePath = toRemotePath(m_workDir);
        QString remoteCmd = "cd '" + remotePath.replace("'", "'\\''") + "' 2>/dev/null && " + cmd;

        const auto &cfg = m_sshManager->profileConfig(m_sshManager->activeProfileIndex());
        QStringList args;
        args << "-o" << "StrictHostKeyChecking=accept-new"
             << "-o" << "ConnectTimeout=5"
             << "-o" << "ControlMaster=auto"
             << "-o" << QString("ControlPath=/tmp/vibe-ssh-%1-%r@%h:%p").arg(QCoreApplication::applicationPid())
             << "-o" << "ControlPersist=120"
             << "-p" << QString::number(cfg.port);
        if (!cfg.identityFile.isEmpty() && QFileInfo(cfg.identityFile).exists())
            args << "-i" << cfg.identityFile;
        args << (cfg.user + "@" + cfg.host) << remoteCmd;

        if (!cfg.password.isEmpty()) {
            m_sshManager->setupSshpassEnv(proc, cfg.password);
            proc->start("sshpass", QStringList() << "-e" << "ssh" << args);
        } else {
            proc->start("ssh", args);
        }
    } else {
        proc->setWorkingDirectory(m_workDir);
        proc->start("sh", {"-c", cmd});
    }
}

void GitGraph::setSshInfo(SshManager *mgr, const QString &mountPoint, const QString &remotePrefix)
{
    m_sshManager = mgr;
    m_sshMountPoint = mountPoint;
    m_sshRemotePrefix = remotePrefix;
}

void GitGraph::clearSshInfo()
{
    m_sshManager = nullptr;
    m_sshMountPoint.clear();
    m_sshRemotePrefix.clear();
}

QString GitGraph::toRemotePath(const QString &localPath) const
{
    if (m_sshMountPoint.isEmpty() || !localPath.startsWith(m_sshMountPoint))
        return localPath;
    return localPath.mid(m_sshMountPoint.length());
}

void GitGraph::setViewerFont(const QFont &font)
{
    m_graphView->setGraphFont(font);
}

void GitGraph::setViewerColors(const QColor &bg, const QColor &fg)
{
    m_graphView->setColors(bg, fg);
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }").arg(bg.name()));
}

void GitGraph::runGitCommand(const QStringList &args, const QString &successMsg, const QString &errorMsg)
{
    if (m_workDir.isEmpty()) return;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_workDir);
    connect(proc, &QProcess::finished, this, [this, proc, successMsg, errorMsg](int exitCode) {
        // Restore buttons
        m_fetchBtn->setEnabled(true);
        m_fetchBtn->setText("Fetch");
        m_pullBtn->setEnabled(true);
        m_pullBtn->setText("Pull");
        m_pushBtn->setEnabled(true);
        m_pushBtn->setText("Push");

        if (exitCode == 0) {
            emit outputMessage(successMsg, 3); // ok
            // Refresh graph after successful operation
            refresh(m_workDir);
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit outputMessage(errorMsg + ": " + err, 2); // error
        }
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc]() {
        m_fetchBtn->setEnabled(true);
        m_fetchBtn->setText("Fetch");
        m_pullBtn->setEnabled(true);
        m_pullBtn->setText("Pull");
        m_pushBtn->setEnabled(true);
        m_pushBtn->setText("Push");
        proc->deleteLater();
    });
    proc->start("git", args);
}

void GitGraph::parseTrackingInfo(const QString &raw)
{
    QString output = raw.trimmed();
    if (output.isEmpty()) {
        m_trackingLabel->setText("No branch");
        return;
    }

    if (output.startsWith("## "))
        output = output.mid(3);

    // Take only first line
    output = output.split('\n').first().trimmed();

    QString info;
    int dotIdx = output.indexOf("...");
    if (dotIdx < 0) {
        info = output + "  (no upstream)";
        m_trackingLabel->setStyleSheet("color: #e5c07b; padding: 0 4px;");
    } else {
        QString branch = output.left(dotIdx);
        QString rest = output.mid(dotIdx + 3).trimmed();

        QString upstream, aheadBehind;
        int bracketIdx = rest.indexOf('[');
        if (bracketIdx >= 0) {
            upstream = rest.left(bracketIdx).trimmed();
            aheadBehind = rest.mid(bracketIdx);
        } else {
            upstream = rest.trimmed();
        }

        info = branch + " -> " + upstream;

        if (aheadBehind.contains("ahead") && aheadBehind.contains("behind")) {
            info += "  " + aheadBehind;
            m_trackingLabel->setStyleSheet("color: #e06c75; padding: 0 4px;");
        } else if (aheadBehind.contains("ahead")) {
            info += "  " + aheadBehind;
            m_trackingLabel->setStyleSheet("color: #98c379; padding: 0 4px;");
        } else if (aheadBehind.contains("behind")) {
            info += "  " + aheadBehind;
            m_trackingLabel->setStyleSheet("color: #e5c07b; padding: 0 4px;");
        } else {
            info += "  [up to date]";
            m_trackingLabel->setStyleSheet("color: #888888; padding: 0 4px;");
        }
    }

    m_trackingLabel->setText(info);
}

void GitGraph::loadTrackingInfo()
{
    if (m_workDir.isEmpty()) {
        m_trackingLabel->clear();
        return;
    }

    // Get current branch + tracking info + ahead/behind counts
    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_workDir);
    connect(proc, &QProcess::finished, this, [this, proc]() {
        QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();

        if (output.isEmpty()) {
            m_trackingLabel->setText("No branch");
            return;
        }

        // Parse: ## branch...upstream [ahead N, behind M]
        // or:    ## branch
        QString info;
        if (output.startsWith("## "))
            output = output.mid(3);

        // Split branch...upstream
        int dotIdx = output.indexOf("...");
        if (dotIdx < 0) {
            // No upstream
            QString branch = output.split('\n').first().trimmed();
            info = branch + "  (no upstream)";
            m_trackingLabel->setStyleSheet("color: #e5c07b; padding: 0 4px;");
        } else {
            QString branch = output.left(dotIdx);
            QString rest = output.mid(dotIdx + 3).split('\n').first().trimmed();

            // Extract upstream and ahead/behind
            QString upstream;
            QString aheadBehind;
            int bracketIdx = rest.indexOf('[');
            if (bracketIdx >= 0) {
                upstream = rest.left(bracketIdx).trimmed();
                aheadBehind = rest.mid(bracketIdx);
            } else {
                upstream = rest.trimmed();
            }

            info = branch + " -> " + upstream;

            if (aheadBehind.contains("ahead") && aheadBehind.contains("behind")) {
                info += "  " + aheadBehind;
                m_trackingLabel->setStyleSheet("color: #e06c75; padding: 0 4px;");
            } else if (aheadBehind.contains("ahead")) {
                info += "  " + aheadBehind;
                m_trackingLabel->setStyleSheet("color: #98c379; padding: 0 4px;");
            } else if (aheadBehind.contains("behind")) {
                info += "  " + aheadBehind;
                m_trackingLabel->setStyleSheet("color: #e5c07b; padding: 0 4px;");
            } else {
                info += "  [up to date]";
                m_trackingLabel->setStyleSheet("color: #888888; padding: 0 4px;");
            }
        }

        m_trackingLabel->setText(info);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() {
        proc->deleteLater();
    });
    proc->start("git", {"status", "-sb", "--porcelain=v1"});
}

void GitGraph::loadRemotes()
{
    if (m_workDir.isEmpty()) {
        m_remotes.clear();
        m_remoteBtn->setText("Remotes");
        m_remoteBtn->setToolTip("No remotes configured");
        return;
    }

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_workDir);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc]() {
        m_remotes.clear();
        QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        // Format: "name\turl (fetch)\nname\turl (push)\n..."
        for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
            if (!line.contains("(fetch)")) continue;
            int tabIdx = line.indexOf('\t');
            if (tabIdx < 0) continue;
            QString name = line.left(tabIdx).trimmed();
            QString url = line.mid(tabIdx + 1).trimmed();
            url.remove(QRegularExpression("\\s*\\(fetch\\)\\s*$"));
            m_remotes[name] = url;
        }

        if (m_remotes.isEmpty()) {
            m_remoteBtn->setText("Remotes");
            m_remoteBtn->setToolTip("No remotes configured. Click to add.");
        } else {
            m_remoteBtn->setText(QString("Remotes (%1)").arg(m_remotes.size()));
            QStringList tip;
            for (auto it = m_remotes.begin(); it != m_remotes.end(); ++it)
                tip << it.key() + ": " + it.value();
            m_remoteBtn->setToolTip(tip.join('\n'));
        }
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() {
        proc->deleteLater();
    });
    proc->start("git", {"remote", "-v"});
}

void GitGraph::showRemotesDialog()
{
    if (m_workDir.isEmpty()) return;

    // Build a dialog with a list of remotes + add/edit/remove
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Git Remotes");
    dlg->setMinimumSize(500, 300);

    auto *layout = new QVBoxLayout(dlg);

    // List of remotes
    auto *list = new QListWidget;
    for (auto it = m_remotes.begin(); it != m_remotes.end(); ++it) {
        auto *item = new QListWidgetItem(it.key() + "  —  " + it.value());
        item->setData(Qt::UserRole, it.key());
        item->setData(Qt::UserRole + 1, it.value());
        list->addItem(item);
    }
    layout->addWidget(list, 1);

    // Input row for name + url
    auto *inputLayout = new QHBoxLayout;
    auto *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText("Name (e.g. origin)");
    nameEdit->setMaximumWidth(120);
    auto *urlEdit = new QLineEdit;
    urlEdit->setPlaceholderText("URL (e.g. git@github.com:user/repo.git)");
    inputLayout->addWidget(nameEdit);
    inputLayout->addWidget(urlEdit, 1);
    layout->addLayout(inputLayout);

    // Buttons
    auto *btnLayout = new QHBoxLayout;
    auto *addBtn = new QPushButton("Add");
    auto *editBtn = new QPushButton("Save");
    auto *removeBtn = new QPushButton("Remove");
    auto *closeBtn = new QPushButton("Close");
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    // Select fills inputs
    connect(list, &QListWidget::currentItemChanged, dlg, [nameEdit, urlEdit](QListWidgetItem *item) {
        if (!item) return;
        nameEdit->setText(item->data(Qt::UserRole).toString());
        urlEdit->setText(item->data(Qt::UserRole + 1).toString());
    });

    // Add remote
    connect(addBtn, &QPushButton::clicked, dlg, [this, nameEdit, urlEdit, list, dlg]() {
        QString name = nameEdit->text().trimmed();
        QString url = urlEdit->text().trimmed();
        if (name.isEmpty() || url.isEmpty()) return;

        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(m_workDir);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, name, url, list, nameEdit, urlEdit](int exitCode) {
            if (exitCode == 0) {
                m_remotes[name] = url;
                auto *item = new QListWidgetItem(name + "  —  " + url);
                item->setData(Qt::UserRole, name);
                item->setData(Qt::UserRole + 1, url);
                list->addItem(item);
                nameEdit->clear();
                urlEdit->clear();
                emit outputMessage("Remote added: " + name, 3);
            } else {
                emit outputMessage("Failed to add remote: " + name, 2);
            }
            proc->deleteLater();
        });
        connect(proc, &QProcess::errorOccurred, this, [proc]() {
            proc->deleteLater();
        });
        proc->start("git", {"remote", "add", name, url});
    });

    // Edit (set-url)
    connect(editBtn, &QPushButton::clicked, dlg, [this, nameEdit, urlEdit, list]() {
        auto *item = list->currentItem();
        if (!item) return;
        QString oldName = item->data(Qt::UserRole).toString();
        QString newName = nameEdit->text().trimmed();
        QString newUrl = urlEdit->text().trimmed();
        if (newName.isEmpty() || newUrl.isEmpty()) return;

        // If name changed, rename first then set-url; otherwise just set-url
        auto doSetUrl = [this, list, item, newName, newUrl]() {
            auto *proc = new QProcess(this);
            proc->setWorkingDirectory(m_workDir);
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, item, newName, newUrl](int exitCode) {
                if (exitCode == 0) {
                    m_remotes[newName] = newUrl;
                    item->setText(newName + "  —  " + newUrl);
                    item->setData(Qt::UserRole, newName);
                    item->setData(Qt::UserRole + 1, newUrl);
                    emit outputMessage("Remote updated: " + newName, 3);
                } else {
                    emit outputMessage("Failed to update remote URL", 2);
                }
                proc->deleteLater();
            });
            connect(proc, &QProcess::errorOccurred, this, [proc]() {
                proc->deleteLater();
            });
            proc->start("git", {"remote", "set-url", newName, newUrl});
        };

        if (oldName != newName) {
            auto *renameProc = new QProcess(this);
            renameProc->setWorkingDirectory(m_workDir);
            connect(renameProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, renameProc, oldName, newName, doSetUrl](int exitCode) {
                if (exitCode == 0) {
                    m_remotes.remove(oldName);
                    doSetUrl();
                } else {
                    emit outputMessage("Failed to rename remote", 2);
                }
                renameProc->deleteLater();
            });
            connect(renameProc, &QProcess::errorOccurred, this, [renameProc]() {
                renameProc->deleteLater();
            });
            renameProc->start("git", {"remote", "rename", oldName, newName});
        } else {
            doSetUrl();
        }
    });

    // Remove
    connect(removeBtn, &QPushButton::clicked, dlg, [this, list, nameEdit, urlEdit]() {
        auto *item = list->currentItem();
        if (!item) return;
        QString name = item->data(Qt::UserRole).toString();

        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(m_workDir);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, name, list, item, nameEdit, urlEdit](int exitCode) {
            if (exitCode == 0) {
                m_remotes.remove(name);
                delete list->takeItem(list->row(item));
                nameEdit->clear();
                urlEdit->clear();
                emit outputMessage("Remote removed: " + name, 3);
            } else {
                emit outputMessage("Failed to remove remote: " + name, 2);
            }
            proc->deleteLater();
        });
        connect(proc, &QProcess::errorOccurred, this, [proc]() {
            proc->deleteLater();
        });
        proc->start("git", {"remote", "remove", name});
    });

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    ThemedDialog::apply(dlg, "Git Remotes");
    dlg->exec();

    // Refresh after dialog closes
    refresh(m_workDir);
    dlg->deleteLater();
}

void GitGraph::loadUserInfo()
{
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int) {
        QString name = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();

        auto *proc2 = new QProcess(this);
        connect(proc2, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc2, name](int) {
            QString email = QString::fromUtf8(proc2->readAllStandardOutput()).trimmed();
            proc2->deleteLater();

            if (name.isEmpty() && email.isEmpty())
                m_userBtn->setText("User (not set)");
            else
                m_userBtn->setText(name + " <" + email + ">");
            m_userBtn->setToolTip("Git user: " + name + " <" + email + ">\nClick to change.");
        });
        connect(proc2, &QProcess::errorOccurred, this, [proc2]() { proc2->deleteLater(); });
        proc2->start("git", {"config", "--global", "user.email"});
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() { proc->deleteLater(); });
    proc->start("git", {"config", "--global", "user.name"});
}

void GitGraph::showUserDialog()
{
    // Read current values synchronously for dialog defaults
    QProcess p;
    p.start("git", {"config", "--global", "user.name"});
    p.waitForFinished(3000);
    QString curName = QString::fromUtf8(p.readAllStandardOutput()).trimmed();

    QProcess p2;
    p2.start("git", {"config", "--global", "user.email"});
    p2.waitForFinished(3000);
    QString curEmail = QString::fromUtf8(p2.readAllStandardOutput()).trimmed();

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Git User");
    dlg->setMinimumWidth(400);
    auto *layout = new QVBoxLayout(dlg);

    layout->addWidget(new QLabel("Name:"));
    auto *nameEdit = new QLineEdit;
    nameEdit->setText(curName);
    layout->addWidget(nameEdit);

    layout->addWidget(new QLabel("Email:"));
    auto *emailEdit = new QLineEdit;
    emailEdit->setText(curEmail);
    layout->addWidget(emailEdit);

    auto *btnLayout = new QHBoxLayout;
    auto *okBtn = new QPushButton("OK");
    auto *cancelBtn = new QPushButton("Cancel");
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    ThemedDialog::apply(dlg, "Git User");

    if (dlg->exec() == QDialog::Accepted) {
        QString newName = nameEdit->text().trimmed();
        QString newEmail = emailEdit->text().trimmed();

        // Run git config asynchronously to avoid blocking the UI
        int pending = 0;
        auto checkDone = [this, &pending]() {
            // Note: captured by value via shared counter
        };

        auto runAsync = [this](const QStringList &args, std::function<void()> onDone) {
            auto *proc = new QProcess(this);
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [proc, onDone]() {
                proc->deleteLater();
                if (onDone) onDone();
            });
            proc->start("git", args);
        };

        bool nameChanged = (newName != curName && !newName.isEmpty());
        bool emailChanged = (newEmail != curEmail && !newEmail.isEmpty());
        int total = (nameChanged ? 1 : 0) + (emailChanged ? 1 : 0);

        if (total == 0) return;

        auto remaining = std::make_shared<int>(total);
        auto finish = [this, remaining]() {
            if (--(*remaining) == 0) {
                refresh(m_workDir);
                emit outputMessage("Git user updated", 3);
            }
        };

        if (nameChanged)
            runAsync({"config", "--global", "user.name", newName}, finish);
        if (emailChanged)
            runAsync({"config", "--global", "user.email", newEmail}, finish);
    }

    dlg->deleteLater();
}

void GitGraph::loadBranches()
{
    if (m_workDir.isEmpty()) return;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_workDir);
    connect(proc, &QProcess::finished, this, [this, proc]() {
        QString current = m_branchCombo->currentText();
        m_branchCombo->blockSignals(true);
        m_branchCombo->clear();
        m_branchCombo->addItem("--all--");

        QString output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QStringList localBranches, remoteBranches;

        for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
            QString branch = line.trimmed();
            if (branch.startsWith("* "))
                branch = branch.mid(2);
            if (branch.isEmpty() || branch.startsWith("("))
                continue;

            if (branch.startsWith("remotes/")) {
                QString remote = branch.mid(8); // strip "remotes/"
                if (!remote.contains("/HEAD"))
                    remoteBranches.append(remote);
            } else {
                localBranches.append(branch);
            }
        }

        // Add local branches
        for (const auto &b : localBranches)
            m_branchCombo->addItem(b);

        // Separator + remote branches
        if (!remoteBranches.isEmpty()) {
            m_branchCombo->insertSeparator(m_branchCombo->count());
            for (const auto &b : remoteBranches)
                m_branchCombo->addItem(b);
        }

        int idx = m_branchCombo->findText(current);
        m_branchCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        m_branchCombo->blockSignals(false);

        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, this, [proc]() {
        proc->deleteLater();
    });
    // -a to include remote branches
    proc->start("git", {"branch", "-a"});
}

void GitGraph::loadLog()
{
    if (m_workDir.isEmpty()) return;

    // First, get set of local branch tips to determine remote-only commits
    auto *localProc = new QProcess(this);
    localProc->setWorkingDirectory(m_workDir);
    connect(localProc, &QProcess::finished, this, [this, localProc]() {
        // Collect local commit hashes
        QSet<QString> localHashes;
        QString localOutput = QString::fromUtf8(localProc->readAllStandardOutput()).trimmed();
        for (const auto &line : localOutput.split('\n', Qt::SkipEmptyParts))
            localHashes.insert(line.trimmed());
        localProc->deleteLater();

        // Now load full log
        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(m_workDir);
        connect(proc, &QProcess::finished, this, [this, proc, localHashes]() {
            QString output = QString::fromUtf8(proc->readAllStandardOutput());
            auto commits = parseGitLog(output);

            // Mark remote-only commits: those not reachable from local branches
            // Simple heuristic: if a commit's refs only contain remote refs
            // and it's not an ancestor of a local ref, mark it
            QSet<QString> localReachable;
            for (const auto &h : localHashes)
                localReachable.insert(h);

            // Walk parents from local commits to mark reachable
            bool changed = true;
            while (changed) {
                changed = false;
                for (const auto &c : commits) {
                    if (localReachable.contains(c.hash)) {
                        for (const auto &p : c.parents) {
                            if (!localReachable.contains(p)) {
                                localReachable.insert(p);
                                changed = true;
                            }
                        }
                    }
                }
            }

            for (auto &c : commits) {
                if (!localReachable.contains(c.hash))
                    c.isRemoteOnly = true;
            }

            m_graphView->setCommits(commits);
            proc->deleteLater();
        });
        connect(proc, &QProcess::errorOccurred, this, [proc]() {
            proc->deleteLater();
        });

        QStringList args = {"log", "--format=%H|%h|%P|%D|%an|%cr|%s", "-n", "200"};
        QString branch = m_branchCombo->currentText();
        if (branch == "--all--")
            args << "--all";
        else
            args << branch;
        proc->start("git", args);
    });
    connect(localProc, &QProcess::errorOccurred, this, [localProc]() {
        localProc->deleteLater();
    });
    // Get all local branch tip hashes
    localProc->start("git", {"rev-list", "--branches", "-n", "200"});
}

QVector<GitCommitInfo> GitGraph::parseGitLog(const QString &output)
{
    QVector<GitCommitInfo> commits;

    for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split('|');
        if (parts.size() < 7) continue;

        GitCommitInfo ci;
        ci.hash = parts[0].trimmed();
        ci.shortHash = parts[1].trimmed();

        QString parentStr = parts[2].trimmed();
        if (!parentStr.isEmpty())
            ci.parents = parentStr.split(' ', Qt::SkipEmptyParts);

        QString refStr = parts[3].trimmed();
        if (!refStr.isEmpty()) {
            for (auto ref : refStr.split(',', Qt::SkipEmptyParts)) {
                ref = ref.trimmed();
                if (ref.contains(" -> ")) {
                    auto sub = ref.split(" -> ");
                    for (auto &s : sub)
                        ci.refs.append(s.trimmed());
                } else {
                    ci.refs.append(ref);
                }
            }
        }

        ci.author = parts[4].trimmed();
        ci.date = parts[5].trimmed();
        ci.subject = QStringList(parts.mid(6)).join('|').trimmed();

        commits.append(ci);
    }

    return commits;
}
