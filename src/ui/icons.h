#pragma once

#include <QColor>
#include <QIcon>

namespace webcam {
enum class Icon { Camera, Refresh, Play, Pause, Chevron, Close, Reset };
QIcon themedIcon(Icon icon, const QColor &color, qreal scale = 1, int logicalSize = 24);
}
