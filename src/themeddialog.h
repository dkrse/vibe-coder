#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QWindow>

// A frameless dialog with a custom title bar that inherits the app theme.
// Subclass this instead of QDialog, or call ThemedDialog::applyTo(dialog).
class DialogTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit DialogTitleBar(const QString &title, QWidget *parent = nullptr);
    void setTitle(const QString &title);

signals:
    void closeClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel;
};

namespace ThemedDialog {
    // Make a QDialog frameless and prepend a custom title bar.
    // Call this at the START of the dialog constructor, before adding content.
    // Returns a QVBoxLayout that the caller should add their content to.
    DialogTitleBar *apply(QDialog *dlg, const QString &title);
}

// Drop-in replacement for QMessageBox that uses frameless themed dialog.
class ThemedMessageBox : public QDialog {
    Q_OBJECT
public:
    enum StandardButton { Save = 0x1, Discard = 0x2, Cancel = 0x4,
                          Yes = 0x8, No = 0x10, Ok = 0x20, SaveAll = 0x40 };
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    explicit ThemedMessageBox(const QString &title, const QString &text,
                              StandardButtons buttons, StandardButton defaultButton,
                              QWidget *parent = nullptr);

    StandardButton clickedButton() const { return m_clicked; }

    // Convenience static methods matching QMessageBox API
    static StandardButton question(QWidget *parent, const QString &title,
                                   const QString &text,
                                   StandardButtons buttons = StandardButtons(Yes | No),
                                   StandardButton defaultButton = No);
    static StandardButton warning(QWidget *parent, const QString &title,
                                  const QString &text,
                                  StandardButtons buttons = Ok,
                                  StandardButton defaultButton = Ok);
    static void information(QWidget *parent, const QString &title,
                            const QString &text);

private:
    StandardButton m_clicked = Cancel;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ThemedMessageBox::StandardButtons)
