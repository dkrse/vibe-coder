#include "notificationpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

NotificationPanel::NotificationPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto *toolbar = new QHBoxLayout;
    m_countLabel = new QLabel("0 entries");
    m_clearBtn = new QPushButton("Clear");
    toolbar->addWidget(m_countLabel);
    toolbar->addStretch();
    toolbar->addWidget(m_clearBtn);
    layout->addLayout(toolbar);

    m_list = new QListWidget;
    m_list->setWordWrap(true);
    layout->addWidget(m_list, 1);

    connect(m_clearBtn, &QPushButton::clicked, this, &NotificationPanel::clear);
}

void NotificationPanel::addInfo(const QString &msg)    { addEntry(LogEntry::Info, msg); }
void NotificationPanel::addWarning(const QString &msg) { addEntry(LogEntry::Warning, msg); }
void NotificationPanel::addError(const QString &msg)   { addEntry(LogEntry::Error, msg); }
void NotificationPanel::addSuccess(const QString &msg) { addEntry(LogEntry::Success, msg); }

void NotificationPanel::clear()
{
    m_list->clear();
    m_unread = 0;
    m_countLabel->setText("0 entries");
    emit unreadChanged(0);
}

void NotificationPanel::addEntry(LogEntry::Level level, const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString text = QString("[%1] %2 %3").arg(time, levelIcon(level), msg);

    auto *item = new QListWidgetItem(text);
    item->setForeground(levelColor(level));
    item->setFont(m_list->font());
    m_list->addItem(item);
    m_list->scrollToBottom();

    m_unread++;
    m_countLabel->setText(QString("%1 entries").arg(m_list->count()));
    emit unreadChanged(m_unread);
}

QString NotificationPanel::levelIcon(LogEntry::Level level) const
{
    switch (level) {
    case LogEntry::Info:    return "[INFO]";
    case LogEntry::Warning: return "[WARN]";
    case LogEntry::Error:   return "[ERR ]";
    case LogEntry::Success: return "[ OK ]";
    }
    return "";
}

QColor NotificationPanel::levelColor(LogEntry::Level level) const
{
    switch (level) {
    case LogEntry::Info:    return QColor("#aaaaaa");
    case LogEntry::Warning: return QColor("#e5a50a");
    case LogEntry::Error:   return QColor("#f44747");
    case LogEntry::Success: return QColor("#4ec9b0");
    }
    return QColor("#cccccc");
}
