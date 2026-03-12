#include "promptedit.h"

PromptEdit::PromptEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
}

void PromptEdit::setSendOnEnter(bool enterOnly)
{
    m_sendOnEnter = enterOnly;
}

void PromptEdit::keyPressEvent(QKeyEvent *event)
{
    bool isEnter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
    bool ctrlHeld = event->modifiers() & Qt::ControlModifier;

    if (m_sendOnEnter) {
        // Enter sends, Shift+Enter = newline
        if (isEnter && !ctrlHeld && !(event->modifiers() & Qt::ShiftModifier)) {
            emit sendRequested();
            return;
        }
        if (isEnter && (event->modifiers() & Qt::ShiftModifier)) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }
    } else {
        // Ctrl+Enter sends, Enter = newline
        if (isEnter && ctrlHeld) {
            emit sendRequested();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}
