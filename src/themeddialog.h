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
