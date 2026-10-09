#include "theme_watcher.h"
#include "core/storage.h"
#include "core/theme_source.h"

#include <QFileInfo>
#include <QLockFile>

namespace webcam {
ThemeWatcher::ThemeWatcher(QString path, Theme theme, QObject *parent)
    : QObject(parent), path_(std::move(path)), theme_(std::move(theme)) {
    debounce_.setSingleShot(true);
    auto schedule = [this] { retries_ = 0; debounce_.start(35); };
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, schedule);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, schedule);
    connect(&debounce_, &QTimer::timeout, this, &ThemeWatcher::reload);
    updateWatches();
    QTimer::singleShot(0, this, &ThemeWatcher::reload);
}

void ThemeWatcher::updateWatches() {
    auto wanted = sourceWatchPaths(theme_.source);
    wanted.append(QFileInfo(path_).absolutePath());
    if (QFileInfo::exists(path_))
        wanted.append(path_);
    wanted.removeDuplicates();
    const auto existing = watcher_.files() + watcher_.directories();
    for (const auto &path : existing)
        if (!wanted.contains(path))
            watcher_.removePath(path);
    for (const auto &path : wanted)
        if (!existing.contains(path) && QFileInfo::exists(path))
            watcher_.addPath(path);
}

void ThemeWatcher::reload() {
    try {
        auto next = loadTheme(path_);
        if (!next.source.isEmpty()) {
            if (QFileInfo(next.source).canonicalFilePath() == QFileInfo(path_).canonicalFilePath())
                throw std::invalid_argument("A theme cannot follow its own output file");
            auto refreshed = followPalette(next.source);
            if (refreshed != next) {
                QLockFile lock(path_ + ".lock");
                if (!lock.tryLock())
                    throw std::runtime_error("Cannot lock theme.json for palette synchronization");
                if (loadTheme(path_) != next) {
                    debounce_.start(60);
                    return;
                }
                writeJson(path_, refreshed.toJson());
            }
            next = std::move(refreshed);
        }
        if (next != theme_) {
            theme_ = std::move(next);
            emit changed();
        }
        retries_ = 0;
    } catch (const std::exception &error) {
        emit rejected(QString::fromUtf8(error.what()));
        // Palette generators sometimes replace several source files in succession.
        if (++retries_ <= 3)
            debounce_.start(100);
    }
    updateWatches();
}
}
