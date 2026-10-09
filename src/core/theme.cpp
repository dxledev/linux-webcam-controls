#include "theme.h"
#include "storage.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <stdexcept>

namespace webcam {
QStringList colorKeys() {
    return {"background", "surface", "surface_alt", "text", "muted", "accent", "accent_text", "border", "danger"};
}

QStringList presetNames() { return {"slate", "paper", "forest", "rose"}; }

Theme presetTheme(const QString &name) {
    static const QMap<QString, QStringList> presets{
        {"slate", {"#14171e", "#1b2029", "#252c38", "#edf0f7", "#a1acbf", "#a8c3f0", "#18283e", "#343e4d", "#f0a6ae"}},
        {"paper", {"#f0efe9", "#faf9f5", "#e6e6df", "#252c2c", "#626e6c", "#356d62", "#ffffff", "#d2d7cd", "#a23848"}},
        {"forest", {"#111b19", "#192622", "#23332c", "#eaf2eb", "#9bad9f", "#b2d8ba", "#203628", "#35483b", "#eba7a5"}},
        {"rose", {"#211a22", "#2c232e", "#382d3b", "#f7eaf1", "#b8a0b2", "#efb7ce", "#3a2131", "#4e3a4c", "#ef9eac"}},
    };
    if (!presets.contains(name))
        throw std::invalid_argument(("Unknown preset: " + name).toStdString());
    Theme theme{name, {}, {}};
    const auto keys = colorKeys();
    for (int index = 0; index < keys.size(); ++index)
        theme.colors.insert(keys[index], QColor(presets[name][index]));
    return theme;
}

QJsonObject Theme::toJson() const {
    QJsonObject palette;
    for (const auto &key : colorKeys())
        palette.insert(key, hex(key));
    QJsonObject result{{"version", 1}, {"name", name}, {"colors", palette}};
    if (!source.isEmpty())
        result.insert("source", source);
    return result;
}

Theme parseTheme(const QJsonObject &object) {
    const auto name = object.value("name").toString().trimmed();
    if (object.value("version").toInt(-1) != 1 || name.isEmpty() || name.size() > 120 || !object.value("colors").isObject())
        throw std::invalid_argument("Theme needs version 1, a name, and a colors object");
    Theme theme{name, {}, {}};
    const auto colors = object.value("colors").toObject();
    static const QRegularExpression validColor("^#[0-9a-fA-F]{6}$");
    for (const auto &key : colorKeys()) {
        const auto text = colors.value(key).toString();
        if (!validColor.match(text).hasMatch())
            throw std::invalid_argument((key + ": expected #RRGGBB").toStdString());
        theme.colors.insert(key, QColor(text));
    }
    for (auto entry = colors.begin(); entry != colors.end(); ++entry)
        if (!colorKeys().contains(entry.key()))
            throw std::invalid_argument(("Unknown color: " + entry.key()).toStdString());
    if (object.contains("source")) {
        theme.source = object.value("source").toString();
        if (theme.source.isEmpty() || !QFileInfo(theme.source).isAbsolute())
            throw std::invalid_argument("Theme source must be an absolute file or directory path");
    }
    return theme;
}

Theme loadTheme(const QString &path) { return parseTheme(readJson(path)); }

Theme ensureTheme(const QString &path) {
    if (QFileInfo::exists(path))
        return loadTheme(path);
    auto theme = presetTheme("slate");
    writeJson(path, theme.toJson());
    return theme;
}

Theme setColors(Theme theme, const QStringList &assignments) {
    if (assignments.isEmpty())
        throw std::invalid_argument("Provide at least one COLOR=#RRGGBB assignment");
    auto object = theme.toJson();
    auto colors = object.value("colors").toObject();
    for (const auto &assignment : assignments) {
        const auto separator = assignment.indexOf('=');
        if (separator < 1 || !colorKeys().contains(assignment.left(separator)))
            throw std::invalid_argument(("Unknown color assignment: " + assignment).toStdString());
        colors.insert(assignment.left(separator), assignment.mid(separator + 1));
    }
    object.insert("name", "custom");
    object.insert("colors", colors);
    object.remove("source");
    return parseTheme(object);
}
}
