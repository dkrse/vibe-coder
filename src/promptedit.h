#pragma once

#include <QPlainTextEdit>
#include <QKeyEvent>

class PromptEdit : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit PromptEdit(QWidget *parent = nullptr);

    void setSendOnEnter(bool enterOnly);
    bool sendOnEnter() const { return m_sendOnEnter; }

signals:
    void sendRequested();
    void saveAndSendRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_sendOnEnter = false; // false = Ctrl+Enter, true = Enter
};
