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

// ── ThemedMessageBox ───────────────────────────────────────────────

static QString buttonLabel(ThemedMessageBox::StandardButton btn) {
    switch (btn) {
    case ThemedMessageBox::Save:    return "Save";
    case ThemedMessageBox::SaveAll: return "Save All";
    case ThemedMessageBox::Discard: return "Discard";
    case ThemedMessageBox::Cancel:  return "Cancel";
    case ThemedMessageBox::Yes:     return "Yes";
    case ThemedMessageBox::No:      return "No";
    case ThemedMessageBox::Ok:      return "OK";
    default: return {};
    }
}

ThemedMessageBox::ThemedMessageBox(const QString &title, const QString &text,
                                   StandardButtons buttons, StandardButton defaultButton,
                                   QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setMinimumWidth(420);
    setMinimumHeight(160);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(20);

    auto *label = new QLabel(text);
    label->setWordWrap(true);
    label->setStyleSheet("font-size: 14px; padding: 8px 0;");
    layout->addWidget(label);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    // Iterate standard buttons in display order
    static const StandardButton order[] = { Save, SaveAll, Discard, Yes, No, Ok, Cancel };
    for (auto b : order) {
        if (!(buttons & b)) continue;
        auto *btn = new QPushButton(buttonLabel(b));
        if (b == defaultButton)
            btn->setDefault(true);
        connect(btn, &QPushButton::clicked, this, [this, b]() {
            m_clicked = b;
            accept();
        });
        btnLayout->addWidget(btn);
    }
    layout->addLayout(btnLayout);

    ThemedDialog::apply(this, title);
}

ThemedMessageBox::StandardButton
ThemedMessageBox::question(QWidget *parent, const QString &title,
                           const QString &text, StandardButtons buttons,
                           StandardButton defaultButton)
{
    ThemedMessageBox dlg(title, text, buttons, defaultButton, parent);
    dlg.exec();
    return dlg.clickedButton();
}

ThemedMessageBox::StandardButton
ThemedMessageBox::warning(QWidget *parent, const QString &title,
                          const QString &text, StandardButtons buttons,
                          StandardButton defaultButton)
{
    ThemedMessageBox dlg(title, text, buttons, defaultButton, parent);
    dlg.exec();
    return dlg.clickedButton();
}

void ThemedMessageBox::information(QWidget *parent, const QString &title,
                                   const QString &text)
{
    ThemedMessageBox dlg(title, text, Ok, Ok, parent);
    dlg.exec();
}
