#include "titlebar.h"
#include <QWindow>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(32);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 4, 0);
    layout->setSpacing(0);

    m_titleLabel = new QLabel("Vibe Coder");
    m_titleLabel->setStyleSheet("font-weight: 500;");
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    auto makeBtn = [this](const QString &text) {
        auto *btn = new QPushButton(text, this);
        btn->setFixedSize(40, 28);
        btn->setFlat(true);
        btn->setStyleSheet(
            "QPushButton { border: none; border-radius: 0; font-size: 14px; padding: 0; }"
            "QPushButton:hover { background-color: rgba(128,128,128,0.3); }");
        return btn;
    };

    m_minimizeBtn = makeBtn("\u2013");  // en-dash as minimize icon
    m_maximizeBtn = makeBtn("\u25A1");  // white square as maximize icon
    m_closeBtn = makeBtn("\u2715");     // multiplication X as close icon
    m_closeBtn->setStyleSheet(
        "QPushButton { border: none; border-radius: 0; font-size: 14px; padding: 0; }"
        "QPushButton:hover { background-color: #e81123; color: white; }");

    layout->addWidget(m_minimizeBtn);
    layout->addWidget(m_maximizeBtn);
    layout->addWidget(m_closeBtn);

    connect(m_minimizeBtn, &QPushButton::clicked, this, &TitleBar::minimizeClicked);
    connect(m_maximizeBtn, &QPushButton::clicked, this, &TitleBar::maximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeClicked);
}

void TitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeClicked();
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Start native window drag
        if (auto *win = window()->windowHandle())
            win->startSystemMove();
    }
}
