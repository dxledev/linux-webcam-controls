#pragma once

#include "core/theme.h"
#include <QFileSystemWatcher>
#include <QTimer>

namespace webcam {
class ThemeWatcher : public QObject {
    Q_OBJECT
public:
    ThemeWatcher(QString path, Theme theme, QObject *parent = nullptr);
    const Theme &theme() const { return theme_; }
    void reload();
signals:
    void changed();
    void rejected(const QString &message);
private:
    QString path_;
    Theme theme_;
    QFileSystemWatcher watcher_;
    QTimer debounce_;
    int retries_ = 0;
    void updateWatches();
};
}
