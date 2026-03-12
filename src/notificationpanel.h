#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>

struct LogEntry {
    enum Level { Info, Warning, Error, Success };
    Level level;
    QString message;
    QDateTime timestamp;
};

class NotificationPanel : public QWidget {
    Q_OBJECT
public:
    explicit NotificationPanel(QWidget *parent = nullptr);

    void addInfo(const QString &msg);
    void addWarning(const QString &msg);
    void addError(const QString &msg);
    void addSuccess(const QString &msg);
    void clear();

    int unreadCount() const { return m_unread; }

signals:
    void unreadChanged(int count);

private:
    void addEntry(LogEntry::Level level, const QString &msg);
    QString levelIcon(LogEntry::Level level) const;
    QColor levelColor(LogEntry::Level level) const;

    QListWidget *m_list;
    QPushButton *m_clearBtn;
    QLabel *m_countLabel;
    int m_unread = 0;
};
