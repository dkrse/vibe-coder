#include "terminalwidget.h"
#include <QVBoxLayout>
#include <QFont>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    m_term = new QTermWidget(1, this); // 1 = start shell immediately
    m_term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    m_term->setTerminalFont(QFont("Monospace", 10));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_term);
}

void TerminalWidget::sendText(const QString &text)
{
    m_term->sendText(text + "\r");
}
