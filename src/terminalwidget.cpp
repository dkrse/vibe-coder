#include "terminalwidget.h"
#include <QVBoxLayout>
#include <QFont>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    m_term = new QTermWidget(0, this); // 0 = don't start shell yet
    m_term->setScrollBarPosition(QTermWidget::ScrollBarRight);
    QFont defaultFont;
    defaultFont.setFamily("Monospace");
    defaultFont.setPointSize(10);
    defaultFont.setStyleHint(QFont::Monospace);
    defaultFont.setFixedPitch(true);
    m_term->setTerminalFont(defaultFont);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_term);

    m_term->startShellProgram(); // start shell after widget is in layout
}

void TerminalWidget::sendText(const QString &text)
{
    m_term->sendText(text + "\r");
}
