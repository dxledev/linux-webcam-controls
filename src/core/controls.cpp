#include "controls.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <climits>
#include <stdexcept>

namespace webcam {
namespace {
QString groupFor(const QString &name, const QString &fallback) {
    if (name.contains("exposure") || name.contains("gain") || name.contains("backlight"))
        return "Exposure";
    if (name.contains("focus"))
        return "Focus";
    if (name.contains("white_balance") || name.contains("temperature") || name.contains("hue") || name.contains("saturation"))
        return "Color";
    if (name.contains("zoom") || name.contains("pan") || name.contains("tilt") || name.contains("rotate") || name.contains("flip"))
        return "Framing";
    if (name.contains("brightness") || name.contains("contrast") || name.contains("sharpness") || name.contains("gamma") || name.contains("power_line"))
        return "Image";
    return fallback.isEmpty() ? "Device" : fallback;
}

qint64 integer(const QString &text, bool &valid) {
    return text.toLongLong(&valid, text.contains("0x", Qt::CaseInsensitive) ? 16 : 10);
}
}

bool Control::writable() const {
    static const QStringList types{"int", "integer", "int64", "bool", "boolean", "menu", "intmenu", "bitmask", "button", "str", "string"};
    return !flags.contains("read-only") && types.contains(type)
           && (!flags.contains("has-payload") || type == "str" || type == "string");
}

bool Control::blocked() const {
    return !writable() || flags.contains("inactive") || flags.contains("grabbed") || flags.contains("disabled");
}

ControlKind Control::kind() const {
    if (!writable())
        return ControlKind::ReadOnly;
    if (type == "button" || flags.contains("write-only"))
        return ControlKind::Action;
    if (type == "bool" || type == "boolean" || ((type == "int" || type == "integer") && hasRange && minimum == 0 && maximum == 1))
        return ControlKind::Toggle;
    if (type == "menu" || type == "intmenu")
        return options.isEmpty() ? ControlKind::ReadOnly : ControlKind::Menu;
    if ((type == "int" || type == "integer") && hasRange && minimum >= INT_MIN && maximum <= INT_MAX
        && static_cast<quint64>(maximum) - static_cast<quint64>(minimum) <= INT_MAX)
        return ControlKind::Slider;
    return ControlKind::Value;
}

bool Control::canReset() const {
    return !blocked() && hasDefault && kind() != ControlKind::Action && kind() != ControlKind::ReadOnly;
}

QString Control::label() const {
    auto result = name;
    result.replace('_', ' ');
    if (!result.isEmpty())
        result[0] = result[0].toUpper();
    return result;
}

QString Control::limitation() const {
    if (flags.contains("inactive"))
        return "Unavailable in the current camera mode";
    if (flags.contains("grabbed"))
        return "Locked while the camera is streaming";
    if (flags.contains("disabled"))
        return "Disabled by the camera driver";
    if (!writable())
        return flags.contains("read-only") ? "Read only" : "This payload cannot be edited as a scalar value";
    if (kind() == ControlKind::ReadOnly)
        return "No selectable values reported by the camera";
    return {};
}

QString Control::valueLabel() const {
    bool valid = false;
    const auto number = integer(value, valid);
    return valid && options.contains(number) ? options[number] : (value.isEmpty() ? "—" : value);
}

QJsonObject Control::toJson() const {
    QJsonArray choices;
    for (auto choice = options.begin(); choice != options.end(); ++choice)
        choices.append(QJsonObject{{"value", QString::number(choice.key())}, {"label", choice.value()}});
    QJsonObject object{{"name", name}, {"type", type}, {"value", value}, {"group", group},
                       {"flags", QJsonArray::fromStringList(flags)}, {"options", choices}, {"writable", !blocked()}};
    if (hasDefault)
        object.insert("default", defaultValue);
    if (hasRange) {
        object.insert("minimum", QString::number(minimum));
        object.insert("maximum", QString::number(maximum));
        object.insert("step", QString::number(step));
    }
    return object;
}

QVector<Control> parseControls(const QString &output) {
    static const QRegularExpression header(R"(^\s*([A-Za-z0-9_]+)\s+0x[0-9a-fA-F]+\s+\(([^)]+)\)\s*:\s*(.*)$)");
    static const QRegularExpression attributes(R"((\w+)=("(?:\\.|[^"\\])*"|.*?)(?=\s+\w+=|$))");
    static const QRegularExpression menu(R"(^\s+(-?\d+)\s*:\s*(.+)$)");
    QVector<Control> controls;
    QString group;
    for (const auto &line : output.split('\n')) {
        const auto match = header.match(line);
        if (match.hasMatch()) {
            Control control;
            control.name = match.captured(1);
            control.type = match.captured(2).trimmed().toLower();
            control.group = groupFor(control.name, group);
            QMap<QString, QString> fields;
            auto values = attributes.globalMatch(match.captured(3));
            while (values.hasNext()) {
                const auto attribute = values.next();
                auto value = attribute.captured(2).trimmed();
                if (value.startsWith('"') && value.endsWith('"')) {
                    value = value.mid(1, value.size() - 2);
                    value.replace("\\\"", "\"");
                    value.replace("\\\\", "\\");
                }
                fields.insert(attribute.captured(1), value);
            }
            bool minimumValid = false, maximumValid = false, stepValid = false;
            control.minimum = integer(fields.value("min"), minimumValid);
            control.maximum = integer(fields.value("max"), maximumValid);
            control.step = integer(fields.value("step"), stepValid);
            if (!stepValid || control.step < 1)
                control.step = 1;
            control.hasRange = minimumValid && maximumValid && control.minimum <= control.maximum;
            control.hasDefault = fields.contains("default");
            control.defaultValue = fields.value("default");
            control.value = fields.value("value");
            auto flags = fields.value("flags");
            flags.remove(QRegularExpression("\\s"));
            control.flags = flags.split(',', Qt::SkipEmptyParts);
            controls.append(std::move(control));
        } else if (const auto option = menu.match(line); option.hasMatch() && !controls.isEmpty()) {
            bool valid = false;
            const auto number = option.captured(1).toLongLong(&valid);
            if (valid)
                controls.last().options.insert(number, option.captured(2).trimmed());
        } else if (!line.trimmed().isEmpty() && !line.front().isSpace()) {
            group = line.trimmed();
        }
    }
    return controls;
}

bool isCaptureDevice(const QString &info) {
    bool inCapabilities = false, inDeviceCaps = false, sawDeviceCaps = false;
    bool capabilitiesCapture = false, deviceCapture = false;
    for (const auto &line : info.split('\n')) {
        const auto text = line.trimmed();
        if (text.startsWith("Capabilities")) {
            inCapabilities = true;
            inDeviceCaps = false;
        } else if (text.startsWith("Device Caps")) {
            sawDeviceCaps = true;
            inDeviceCaps = true;
            inCapabilities = false;
        } else if (text == "Video Capture" || text == "Video Capture Multiplanar") {
            deviceCapture |= inDeviceCaps;
            capabilitiesCapture |= inCapabilities;
        }
    }
    return sawDeviceCaps ? deviceCapture : capabilitiesCapture;
}

bool validDevice(const QString &path) {
    static const QRegularExpression pattern("^/dev/video[0-9]+$");
    return pattern.match(path).hasMatch();
}

QString validateValue(const Control &control, const QString &value) {
    if (control.blocked())
        throw std::invalid_argument((control.label() + ": " + control.limitation()).toStdString());
    if (value.size() > 4096 || value.contains(QRegularExpression("[\\t\\r\\n,]")))
        throw std::invalid_argument("Control value contains an unsafe separator or exceeds 4096 characters");
    if (control.type == "str" || control.type == "string") {
        if (control.hasRange && (value.size() < control.minimum || value.size() > control.maximum))
            throw std::invalid_argument("String length is outside the camera's range");
        return value;
    }
    static const QRegularExpression numeric("^-?(?:0[xX][0-9a-fA-F]+|[0-9]+)$");
    bool valid = false;
    const auto number = integer(value, valid);
    if (!numeric.match(value).hasMatch() || !valid)
        throw std::invalid_argument("Control requires a decimal or hexadecimal integer");
    if (control.kind() == ControlKind::Action)
        return QString::number(number);
    if (control.hasRange && (number < control.minimum || number > control.maximum))
        throw std::invalid_argument("Value is outside the camera's range");
    if (control.hasRange && control.step > 1
        && (static_cast<quint64>(number) - static_cast<quint64>(control.minimum)) % static_cast<quint64>(control.step) != 0)
        throw std::invalid_argument("Value does not match the camera's step size");
    if ((control.type == "menu" || control.type == "intmenu") && !control.options.contains(number))
        throw std::invalid_argument("Value is not one of the camera's menu entries");
    if ((control.type == "bool" || control.type == "boolean") && number != 0 && number != 1)
        throw std::invalid_argument("Boolean controls require 0 or 1");
    return QString::number(number);
}
}
