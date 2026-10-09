#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <stdexcept>

namespace webcam {
QJsonObject readJson(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        throw std::runtime_error(("Cannot read JSON: " + path).toStdString());
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error(("Invalid JSON: " + path + ": " + error.errorString()).toStdString());
    return document.object();
}

void writeJson(const QString &path, const QJsonObject &object) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        throw std::runtime_error(("Cannot create directory for " + path).toStdString());
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error(("Cannot save JSON: " + path + ": " + file.errorString()).toStdString());
}

QString defaultConfigDirectory() {
    const auto override = qEnvironmentVariable("WEBCAM_CONTROLS_CONFIG_DIR");
    return override.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/webcam-controls"
                              : absolutePath(override);
}

QString absolutePath(QString path) {
    if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    return QFileInfo(path).absoluteFilePath();
}
}
