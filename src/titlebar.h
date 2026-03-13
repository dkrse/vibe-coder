#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setTitle(const QString &title);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel;
    QPushButton *m_minimizeBtn;
    QPushButton *m_maximizeBtn;
    QPushButton *m_closeBtn;
};
