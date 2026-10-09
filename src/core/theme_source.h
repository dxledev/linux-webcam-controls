#pragma once

#include "theme.h"

namespace webcam {
Theme importPalette(const QString &path);
Theme followPalette(const QString &path);
QStringList sourceWatchPaths(const QString &path);
}
