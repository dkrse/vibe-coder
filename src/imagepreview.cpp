#include "imagepreview.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtMath>

ImagePreview::ImagePreview(const QString &filePath, QWidget *parent)
    : QWidget(parent)
{
    m_pixmap.load(filePath);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
}

void ImagePreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Checkerboard background for transparency
    const int sq = 12;
    QColor c1(204, 204, 204), c2(255, 255, 255);
    for (int y = 0; y < height(); y += sq)
        for (int x = 0; x < width(); x += sq) {
            p.fillRect(x, y, sq, sq, ((x / sq + y / sq) & 1) ? c2 : c1);
        }

    if (m_pixmap.isNull()) return;

    double w = m_pixmap.width() * m_zoom;
    double h = m_pixmap.height() * m_zoom;

    // Center if image smaller than viewport
    double dx = m_offset.x();
    double dy = m_offset.y();
    if (w < width())  dx = (width() - w) / 2.0;
    if (h < height()) dy = (height() - h) / 2.0;

    p.drawPixmap(QRectF(dx, dy, w, h), m_pixmap, m_pixmap.rect());
}

void ImagePreview::wheelEvent(QWheelEvent *event)
{
    if (m_pixmap.isNull()) return;

    // Zoom toward cursor position
    QPointF cursorPos = event->position();
    double oldZoom = m_zoom;

    double delta = event->angleDelta().y() / 120.0;
    double factor = qPow(1.15, delta);
    m_zoom = qBound(0.05, m_zoom * factor, 50.0);

    // Adjust offset so the point under the cursor stays fixed
    double ratio = m_zoom / oldZoom;
    m_offset = cursorPos - ratio * (cursorPos - m_offset);

    clampOffset();
    update();
    event->accept();
}

void ImagePreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_offsetStart = m_offset;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImagePreview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_offset = m_offsetStart + (event->pos() - m_dragStart);
        clampOffset();
        update();
    }
}

void ImagePreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void ImagePreview::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Equal || event->key() == Qt::Key_Plus) {
            zoomIn();
            return;
        }
        if (event->key() == Qt::Key_Minus) {
            zoomOut();
            return;
        }
        if (event->key() == Qt::Key_0) {
            zoomReset();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void ImagePreview::zoomIn()
{
    QPointF center(width() / 2.0, height() / 2.0);
    double oldZoom = m_zoom;
    m_zoom = qBound(0.05, m_zoom * 1.25, 50.0);
    double ratio = m_zoom / oldZoom;
    m_offset = center - ratio * (center - m_offset);
    clampOffset();
    update();
}

void ImagePreview::zoomOut()
{
    QPointF center(width() / 2.0, height() / 2.0);
    double oldZoom = m_zoom;
    m_zoom = qBound(0.05, m_zoom / 1.25, 50.0);
    double ratio = m_zoom / oldZoom;
    m_offset = center - ratio * (center - m_offset);
    clampOffset();
    update();
}

void ImagePreview::zoomReset()
{
    m_zoom = 1.0;
    m_offset = QPointF(0, 0);
    update();
}

void ImagePreview::clampOffset()
{
    // Allow some overscroll but not infinite
    double w = m_pixmap.width() * m_zoom;
    double h = m_pixmap.height() * m_zoom;
    double maxX = qMax(w, (double)width());
    double maxY = qMax(h, (double)height());
    m_offset.setX(qBound(-maxX, m_offset.x(), maxX));
    m_offset.setY(qBound(-maxY, m_offset.y(), maxY));
}
