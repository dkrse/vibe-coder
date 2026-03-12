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
    bool shiftHeld = event->modifiers() & Qt::ShiftModifier;

    if (m_sendOnEnter) {
        // Enter sends, Shift+Enter = send+save, Ctrl+Enter/other = newline
        if (isEnter && shiftHeld && !ctrlHeld) {
            emit saveAndSendRequested();
            return;
        }
        if (isEnter && !ctrlHeld && !shiftHeld) {
            emit sendRequested();
            return;
        }
    } else {
        // Ctrl+Enter sends, Ctrl+Shift+Enter = send+save, Enter = newline
        if (isEnter && ctrlHeld && shiftHeld) {
            emit saveAndSendRequested();
            return;
        }
        if (isEnter && ctrlHeld && !shiftHeld) {
            emit sendRequested();
            return;
        }
    }

    QPlainTextEdit::keyPressEvent(event);
}
