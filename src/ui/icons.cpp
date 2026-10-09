#include "icons.h"

#include <QPainter>
#include <QPainterPath>

namespace webcam {
QIcon themedIcon(Icon icon, const QColor &color, qreal scale, int logicalSize) {
    QPixmap pixmap(qRound(logicalSize * scale), qRound(logicalSize * scale));
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.scale(logicalSize / 24.0, logicalSize / 24.0);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (icon) {
    case Icon::Camera:
        painter.drawRoundedRect(QRectF(3, 5, 18, 13), 4, 4);
        painter.drawEllipse(QPointF(12, 11.5), 3.5, 3.5);
        painter.drawLine(QPointF(9, 21), QPointF(15, 21));
        painter.drawLine(QPointF(12, 18), QPointF(12, 21));
        break;
    case Icon::Refresh:
    case Icon::Reset:
        painter.drawArc(QRectF(5, 5, 14, 14), 35 * 16, 285 * 16);
        painter.drawPolyline(QPolygonF{QPointF(19, 4), QPointF(19, 9), QPointF(14, 9)});
        break;
    case Icon::Play:
        painter.drawPolygon(QPolygonF{QPointF(8, 5), QPointF(19, 12), QPointF(8, 19)});
        break;
    case Icon::Pause:
        painter.drawLine(QPointF(9, 6), QPointF(9, 18));
        painter.drawLine(QPointF(15, 6), QPointF(15, 18));
        break;
    case Icon::Chevron:
        painter.drawPolyline(QPolygonF{QPointF(7, 10), QPointF(12, 15), QPointF(17, 10)});
        break;
    case Icon::Close:
        painter.drawLine(QPointF(7, 7), QPointF(17, 17));
        painter.drawLine(QPointF(17, 7), QPointF(7, 17));
        break;
    }
    painter.end();
    return QIcon(pixmap);
}
}
