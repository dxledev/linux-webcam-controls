#pragma once

#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QVector>

namespace webcam {
enum class ControlKind { Toggle, Slider, Menu, Action, Value, ReadOnly };

struct Control {
    QString name;
    QString type;
    QString value;
    QString defaultValue;
    QString group;
    QStringList flags;
    QMap<qint64, QString> options;
    qint64 minimum = 0;
    qint64 maximum = 0;
    qint64 step = 1;
    bool hasRange = false;
    bool hasDefault = false;
    bool writable() const;
    bool blocked() const;
    bool canReset() const;
    ControlKind kind() const;
    QString label() const;
    QString limitation() const;
    QString valueLabel() const;
    QJsonObject toJson() const;
};

QVector<Control> parseControls(const QString &output);
bool isCaptureDevice(const QString &info);
bool validDevice(const QString &path);
QString validateValue(const Control &control, const QString &value);
}
