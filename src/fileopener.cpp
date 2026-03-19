#include "fileopener.h"

#include <QApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <algorithm>

FileOpener::FileOpener(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setFixedWidth(600);
    setMaximumHeight(450);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Search files by name...");
    layout->addWidget(m_input);

    m_list = new QListWidget;
    layout->addWidget(m_list);

    m_input->installEventFilter(this);
    m_list->installEventFilter(this);

    connect(m_input, &QLineEdit::textChanged, this, &FileOpener::filterFiles);
    connect(m_list, &QListWidget::itemActivated, this, [this]() { openSelected(); });
}

void FileOpener::setRootPath(const QString &path)
{
    if (m_rootPath != path) {
        m_rootPath = path;
        m_allFiles.clear(); // force rescan
    }
}

void FileOpener::show()
{
    m_input->clear();
    scanFiles();
    filterFiles("");

    if (parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(
            QPoint(parentWidget()->width() / 2 - width() / 2, 60));
        move(center);
    }

    QWidget::show();
    m_input->setFocus();
    raise();
}

void FileOpener::scanFiles()
{
    if (!m_allFiles.isEmpty()) return; // already scanned

    m_allFiles.clear();
    static const QSet<QString> skipDirs = {
        ".git", "node_modules", "__pycache__", ".LLM", "build", "dist",
        ".cache", ".venv", "venv", ".tox", "target", "cmake-build-debug",
        "cmake-build-release"
    };

    QDirIterator it(m_rootPath, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    int count = 0;
    while (it.hasNext() && count < 50000) {
        it.next();
        QString rel = it.filePath().mid(m_rootPath.length() + 1);

        // Skip hidden/build directories
        bool skip = false;
        for (const auto &sd : skipDirs) {
            if (rel.startsWith(sd + "/") || rel.contains("/" + sd + "/")) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        m_allFiles.append(rel);
        ++count;
    }

    std::sort(m_allFiles.begin(), m_allFiles.end());
}

int FileOpener::fuzzyScore(const QString &pattern, const QString &str) const
{
    if (pattern.isEmpty()) return 0;

    QString lowerStr = str.toLower();
    QString lowerPat = pattern.toLower();

    // Exact substring match gets high score
    int subIdx = lowerStr.indexOf(lowerPat);
    if (subIdx >= 0) {
        int score = 100 - subIdx; // earlier match = higher score
        // Bonus for filename match (not in path)
        if (subIdx >= lowerStr.lastIndexOf('/') + 1)
            score += 50;
        return score;
    }

    // Fuzzy: all chars must appear in order
    int si = 0;
    int matched = 0;
    for (int pi = 0; pi < lowerPat.length(); ++pi) {
        bool found = false;
        while (si < lowerStr.length()) {
            if (lowerStr[si] == lowerPat[pi]) {
                ++matched;
                ++si;
                found = true;
                break;
            }
            ++si;
        }
        if (!found) return -1; // no match
    }

    return matched * 10 - str.length(); // prefer shorter paths
}

void FileOpener::filterFiles(const QString &text)
{
    m_list->clear();

    if (text.isEmpty()) {
        // Show first 100 files
        int limit = qMin(m_allFiles.size(), 100);
        for (int i = 0; i < limit; ++i) {
            auto *item = new QListWidgetItem(m_allFiles[i]);
            item->setData(Qt::UserRole, m_rootPath + "/" + m_allFiles[i]);
            m_list->addItem(item);
        }
    } else {
        // Score and sort
        struct ScoredFile {
            int score;
            int index;
        };
        QVector<ScoredFile> scored;
        scored.reserve(m_allFiles.size());

        for (int i = 0; i < m_allFiles.size(); ++i) {
            int s = fuzzyScore(text, m_allFiles[i]);
            if (s >= 0)
                scored.append({s, i});
        }

        std::sort(scored.begin(), scored.end(), [](const ScoredFile &a, const ScoredFile &b) {
            return a.score > b.score;
        });

        int limit = qMin(scored.size(), 50);
        for (int i = 0; i < limit; ++i) {
            const QString &rel = m_allFiles[scored[i].index];
            auto *item = new QListWidgetItem(rel);
            item->setData(Qt::UserRole, m_rootPath + "/" + rel);
            m_list->addItem(item);
        }
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    int itemHeight = m_list->sizeHintForRow(0);
    if (itemHeight <= 0) itemHeight = 28;
    int listHeight = qMin(m_list->count() * itemHeight + 4, 350);
    m_list->setFixedHeight(listHeight);
    adjustSize();
}

void FileOpener::openSelected()
{
    auto *item = m_list->currentItem();
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString();
    hide();
    emit fileSelected(path);
}

bool FileOpener::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);

        if (ke->key() == Qt::Key_Escape) {
            hide();
            return true;
        }

        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            openSelected();
            return true;
        }

        if (obj == m_input) {
            if (ke->key() == Qt::Key_Down) {
                int row = m_list->currentRow();
                if (row < m_list->count() - 1)
                    m_list->setCurrentRow(row + 1);
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                int row = m_list->currentRow();
                if (row > 0)
                    m_list->setCurrentRow(row - 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
