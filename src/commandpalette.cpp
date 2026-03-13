#include "commandpalette.h"

#include <QApplication>
#include <QScreen>

CommandPalette::CommandPalette(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setFixedWidth(500);
    setMaximumHeight(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Type a command...");
    layout->addWidget(m_input);

    m_list = new QListWidget;
    layout->addWidget(m_list);

    m_input->installEventFilter(this);
    m_list->installEventFilter(this);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::filterCommands);
    connect(m_list, &QListWidget::itemActivated, this, [this]() { executeSelected(); });
}

void CommandPalette::addCommand(const QString &name, const QString &shortcut,
                                 std::function<void()> action)
{
    m_commands.append({name, shortcut, action});
}

void CommandPalette::show()
{
    m_input->clear();
    filterCommands("");

    // Center horizontally at top of parent
    if (parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(
            QPoint(parentWidget()->width() / 2 - width() / 2, 80));
        move(center);
    }

    QWidget::show();
    m_input->setFocus();
    raise();
}

void CommandPalette::filterCommands(const QString &text)
{
    m_list->clear();
    QString filter = text.toLower();

    for (const auto &cmd : m_commands) {
        if (filter.isEmpty() || cmd.name.toLower().contains(filter)) {
            QString label = cmd.name;
            if (!cmd.shortcut.isEmpty())
                label += "    " + cmd.shortcut;
            auto *item = new QListWidgetItem(label);
            item->setData(Qt::UserRole, cmd.name);
            m_list->addItem(item);
        }
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    // Resize height to fit
    int itemHeight = m_list->sizeHintForRow(0);
    if (itemHeight <= 0) itemHeight = 28;
    int listHeight = qMin(m_list->count() * itemHeight + 4, 300);
    m_list->setFixedHeight(listHeight);
    adjustSize();
}

void CommandPalette::executeSelected()
{
    auto *item = m_list->currentItem();
    if (!item) return;

    QString name = item->data(Qt::UserRole).toString();
    hide();

    for (const auto &cmd : m_commands) {
        if (cmd.name == name) {
            cmd.action();
            break;
        }
    }
}

bool CommandPalette::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);

        if (ke->key() == Qt::Key_Escape) {
            hide();
            return true;
        }

        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            executeSelected();
            return true;
        }

        if (obj == m_input) {
            if (ke->key() == Qt::Key_Down) {
                int row = m_list->currentRow();
                if (row < m_list->count() - 1)
                    m_list->setCurrentRow(row + 1);
                return true;
            }
            if (ke->key() == Qt::Key_Up) {
                int row = m_list->currentRow();
                if (row > 0)
                    m_list->setCurrentRow(row - 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
