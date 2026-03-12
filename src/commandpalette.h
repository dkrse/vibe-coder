#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QAction>
#include <QKeyEvent>

struct Command {
    QString name;
    QString shortcut;
    std::function<void()> action;
};

class CommandPalette : public QWidget {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget *parent = nullptr);

    void addCommand(const QString &name, const QString &shortcut, std::function<void()> action);
    void show();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void filterCommands(const QString &text);
    void executeSelected();

    QLineEdit *m_input;
    QListWidget *m_list;
    QVector<Command> m_commands;
};
