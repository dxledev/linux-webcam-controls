#pragma once

#include <QJsonObject>
#include <QString>

namespace webcam {
QJsonObject readJson(const QString &path);
void writeJson(const QString &path, const QJsonObject &object);
QString defaultConfigDirectory();
QString absolutePath(QString path);
}
