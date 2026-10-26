#include "theme_source.h"
#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>
#include <stdexcept>

namespace webcam {
namespace {
using Palette = QMap<QString, QColor>;

QString resolvePalette(const QString &path) {
    if (QFileInfo(path).isFile())
        return path;
    for (const auto &file : {"webcam-controls.json", "Colors.qml", "colors.json", "noctalia.json", "palette.json"})
        if (QFileInfo::exists(path + '/' + file))
            return path + '/' + file;
    throw std::invalid_argument(("No supported palette in " + path).toStdString());
}

QColor mix(const QColor &base, const QColor &tint, double amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                           base.greenF() * (1 - amount) + tint.greenF() * amount,
                           base.blueF() * (1 - amount) + tint.blueF() * amount);
}

QColor pick(const Palette &palette, const QStringList &names, QColor fallback = {}) {
    for (const auto &name : names)
        if (palette.contains(name))
            return palette[name];
    if (fallback.isValid())
        return fallback;
    throw std::invalid_argument(("Palette is missing " + names.join(" / ")).toStdString());
}

Palette jsonColors(QJsonObject object) {
    if (object.value("dark").isObject() || object.value("light").isObject())
        object = object.value(object.value("mode").toString("dark")).toObject();
    for (const auto &container : {"colours", "colors"})
        if (object.value(container).isObject()) {
            object = object.value(container).toObject();
            break;
        }
    Palette palette;
    static const QRegularExpression color("^#?[a-fA-F0-9]{6}$");
    for (auto entry = object.begin(); entry != object.end(); ++entry) {
        auto value = entry.value().toString();
        if (color.match(value).hasMatch()) {
            if (!value.startsWith('#'))
                value.prepend('#');
            palette.insert(entry.key(), QColor(value));
        }
    }
    return palette;
}

double rgbaChannel(const QString &text) {
    const auto parts = text.trimmed().split('/');
    bool valid = false;
    auto value = parts[0].toDouble(&valid);
    if (!valid || parts.size() > 2)
        throw std::invalid_argument("Invalid Qt.rgba channel");
    if (parts.size() == 2) {
        const auto divisor = parts[1].toDouble(&valid);
        if (!valid || divisor <= 0)
            throw std::invalid_argument("Invalid Qt.rgba divisor");
        value /= divisor;
    }
    if (!std::isfinite(value) || value < 0 || value > 1)
        throw std::invalid_argument("Qt.rgba channels must be between 0 and 1");
    return value;
}

Palette qmlColors(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        throw std::invalid_argument(("Cannot read palette: " + path).toStdString());
    auto contents = QString::fromUtf8(file.readAll());
    contents.remove(QRegularExpression("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    contents.remove(QRegularExpression("//[^\\n]*"));
    static const QRegularExpression property(R"(property\s+color\s+(\w+)\s*:\s*(?:["'](#[a-fA-F0-9]{6}(?:[a-fA-F0-9]{2})?)["']|Qt\.rgba\(([^)]+)\)))");
    Palette palette;
    auto matches = property.globalMatch(contents);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (!match.captured(2).isEmpty()) {
            auto color = QColor(match.captured(2));
            color.setAlpha(255);
            palette.insert(match.captured(1), color);
        } else {
            const auto channels = match.captured(3).split(',');
            if (channels.size() != 4)
                throw std::invalid_argument("Qt.rgba needs four channels");
            rgbaChannel(channels[3]);
            palette.insert(match.captured(1), QColor::fromRgbF(rgbaChannel(channels[0]), rgbaChannel(channels[1]), rgbaChannel(channels[2])));
        }
    }
    return palette;
}

Theme mapPalette(const Palette &palette, const QString &name) {
    const auto background = pick(palette, {"background", "base", "m3background", "mSurface", "surface"});
    const auto text = pick(palette, {"foreground", "text", "on_background", "onBackground", "mOnSurface", "on_surface", "onSurface", "m3onSurface"});
    const auto accent = pick(palette, {"accent", "primary", "mPrimary", "m3primary"});
    const auto surface = pick(palette, {"surface_container_low", "surfaceContainerLow", "backgroundAlt", "mantle"}, mix(background, text, .035));
    const auto accentText = accent.lightnessF() > .55 ? QColor("#16191f") : QColor("#ffffff");
    Theme theme{name, {
        {"background", background}, {"surface", surface},
        {"surface_alt", pick(palette, {"surface_alt", "surface_container", "surfaceContainer", "backgroundGray", "mSurfaceVariant", "surface0"}, mix(background, text, .075))},
        {"text", text}, {"muted", pick(palette, {"muted", "foregroundInactive", "on_surface_variant", "onSurfaceVariant", "mOnSurfaceVariant"}, mix(background, text, .65))},
        {"accent", accent}, {"accent_text", pick(palette, {"accent_text", "on_primary", "onPrimary", "mOnPrimary"}, accentText)},
        {"border", mix(surface, pick(palette, {"outline", "border", "mOutline"}, text), .25)},
        {"danger", pick(palette, {"danger", "error", "mError", "m3error"}, mix(text, QColor("#e74960"), .65))},
    }, {}};
    return parseTheme(theme.toJson());
}
}

Theme importPalette(const QString &requested) {
    const auto path = resolvePalette(absolutePath(requested));
    const auto name = QFileInfo(path).dir().dirName();
    if (path.endsWith(".qml", Qt::CaseInsensitive))
        return mapPalette(qmlColors(path), name);
    const auto object = readJson(path);
    if (object.value("version").toInt() == 1 && object.value("colors").toObject().contains("accent_text")) {
        auto theme = parseTheme(object);
        theme.source.clear();
        return theme;
    }
    return mapPalette(jsonColors(object), name);
}

Theme followPalette(const QString &path) {
    auto theme = importPalette(path);
    // Keep the logical path so replacing a selector symlink changes the source.
    theme.source = absolutePath(path);
    return theme;
}

QStringList sourceWatchPaths(const QString &path) {
    if (path.isEmpty())
        return {};
    QStringList paths;
    auto collect = [&paths](QString current) {
        while (!QFileInfo::exists(current) && !QDir(current).isRoot())
            current = QFileInfo(current).dir().absolutePath();
        for (int depth = 0; depth < 3 && !QDir(current).isRoot(); ++depth) {
            if (QFileInfo::exists(current))
                paths.append(current);
            current = QFileInfo(current).dir().absolutePath();
        }
    };
    collect(path);
    if (QFileInfo(path).isDir())
        for (const auto &file : QDir(path).entryList(QDir::Files))
            if (file.endsWith(".json") || file == "Colors.qml")
                collect(path + '/' + file);
    const auto canonical = QFileInfo(path).canonicalFilePath();
    if (!canonical.isEmpty()) {
        collect(canonical);
        if (QFileInfo(canonical).isDir())
            for (const auto &file : QDir(canonical).entryList(QDir::Files))
                if (file.endsWith(".json") || file == "Colors.qml")
                    collect(canonical + '/' + file);
    }
    paths.removeDuplicates();
    return paths;
}
}
