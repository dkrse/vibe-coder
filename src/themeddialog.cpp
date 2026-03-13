#include "themeddialog.h"

// ── DialogTitleBar ──────────────────────────────────────────────────

DialogTitleBar::DialogTitleBar(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(32);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(0);

    m_titleLabel = new QLabel(title);
    m_titleLabel->setStyleSheet("font-size: 13px; font-weight: 500;");
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    auto *closeBtn = new QPushButton("\u2715", this);
    closeBtn->setFixedSize(40, 28);
    closeBtn->setFlat(true);
    closeBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 0; font-size: 14px; padding: 0; }"
        "QPushButton:hover { background-color: #e81123; color: white; }");

    layout->addWidget(closeBtn);

    connect(closeBtn, &QPushButton::clicked, this, &DialogTitleBar::closeClicked);
}

void DialogTitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void DialogTitleBar::mouseDoubleClickEvent(QMouseEvent *) {}

void DialogTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *win = window()->windowHandle())
            win->startSystemMove();
    }
}

// ── ThemedDialog namespace ──────────────────────────────────────────

DialogTitleBar *ThemedDialog::apply(QDialog *dlg, const QString &title)
{
    dlg->setWindowFlag(Qt::FramelessWindowHint);

    auto *bar = new DialogTitleBar(title, dlg);
    QObject::connect(bar, &DialogTitleBar::closeClicked, dlg, &QDialog::reject);

    // Insert the title bar at the top of the dialog's layout.
    // If the dialog already has a layout, wrap it.
    if (auto *existing = dlg->layout()) {
        auto *wrapper = new QVBoxLayout;
        wrapper->setContentsMargins(0, 0, 0, 0);
        wrapper->setSpacing(0);
        wrapper->addWidget(bar);

        // Re-parent all items from existing layout
        auto *content = new QWidget;
        content->setLayout(existing);
        wrapper->addWidget(content, 1);

        delete dlg->layout();
        dlg->setLayout(wrapper);
    } else {
        auto *layout = new QVBoxLayout(dlg);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(bar);
    }

    return bar;
}
