#include "promptedit.h"

PromptEdit::PromptEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &PromptEdit::updateCurrentLineHighlight);
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

void PromptEdit::setHighlightCurrentLine(bool enable)
{
    m_highlightLine = enable;
    updateCurrentLineHighlight();
}

void PromptEdit::updateCurrentLineHighlight()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (m_highlightLine) {
        QTextEdit::ExtraSelection sel;
        // Detect dark/light from current background
        QColor bg = palette().color(QPalette::Base);
        bool dark = (bg.lightness() < 128);
        QColor lineColor = dark ? QColor("#2a2d2e") : QColor("#e8f2fe");
        sel.format.setBackground(lineColor);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        selections.append(sel);
    }
    setExtraSelections(selections);
}
