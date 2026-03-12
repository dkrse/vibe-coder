#pragma once

#include <QWidget>
#include <qtermwidget6/qtermwidget.h>

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);

    void sendText(const QString &text);
    QTermWidget *terminal() const { return m_term; }

private:
    QTermWidget *m_term;
};
