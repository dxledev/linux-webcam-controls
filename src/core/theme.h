#pragma once

#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace webcam {
struct Theme {
    QString name;
    QMap<QString, QColor> colors;
    QString source;
    QColor color(const QString &key) const { return colors.value(key); }
    QString hex(const QString &key) const { return color(key).name(); }
    QJsonObject toJson() const;
    bool operator==(const Theme &) const = default;
};

QStringList colorKeys();
QStringList presetNames();
Theme presetTheme(const QString &name);
Theme parseTheme(const QJsonObject &object);
Theme loadTheme(const QString &path);
Theme ensureTheme(const QString &path);
Theme setColors(Theme theme, const QStringList &assignments);
}
