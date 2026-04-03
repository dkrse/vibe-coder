#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPointF>

class ImagePreview : public QWidget {
    Q_OBJECT
public:
    explicit ImagePreview(const QString &filePath, QWidget *parent = nullptr);

    void zoomIn();
    void zoomOut();
    void zoomReset();
    double zoomFactor() const { return m_zoom; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void clampOffset();

    QPixmap m_pixmap;
    double m_zoom = 1.0;
    QPointF m_offset;      // pan offset in widget coords
    bool m_dragging = false;
    QPoint m_dragStart;
    QPointF m_offsetStart;
};
