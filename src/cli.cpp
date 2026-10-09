#include "cli.h"
#include "core/storage.h"
#include "core/theme_source.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace webcam {
namespace {
void printJson(const QJsonObject &object) {
    QTextStream(stdout) << QJsonDocument(object).toJson(QJsonDocument::Indented);
}

int themeCommand(const Options &options) {
    auto arguments = options.arguments;
    arguments.removeFirst();
    if (arguments.isEmpty())
        throw std::invalid_argument("Usage: webcam-controls theme list|path|show|preset|set|import|export|follow|unfollow");
    const auto action = arguments.takeFirst();
    const auto path = options.window.configDirectory + "/theme.json";
    auto count = [&arguments](int expected) {
        if (arguments.size() != expected)
            throw std::invalid_argument("Incorrect number of theme arguments; see --help");
    };
    if (action == "list") {
        count(0);
        QTextStream(stdout) << presetNames().join('\n') << '\n';
        return 0;
    }
    if (action == "path") {
        count(0);
        QTextStream(stdout) << path << '\n';
        return 0;
    }
    const bool readOnly = action == "show" || action == "export";
    std::unique_ptr<QLockFile> lock;
    if (!options.dryRun && !readOnly) {
        if (!QDir().mkpath(options.window.configDirectory))
            throw std::runtime_error("Cannot create configuration directory");
        lock = std::make_unique<QLockFile>(path + ".lock");
        if (!lock->tryLock(2000))
            throw std::runtime_error("Theme is busy; try again");
    }
    const bool needsExisting = action == "show" || action == "export" || action == "set" || action == "unfollow";
    auto theme = needsExisting && QFileInfo::exists(path) ? loadTheme(path) : presetTheme("slate");
    if (action == "show") {
        count(0);
        printJson(theme.toJson());
        return 0;
    }
    if (action == "preset") {
        count(1);
        theme = presetTheme(arguments.first());
    } else if (action == "set") {
        theme = setColors(theme, arguments);
    } else if (action == "import" || action == "follow") {
        count(1);
        if (absolutePath(arguments.first()) == path ||
            (!QFileInfo(path).canonicalFilePath().isEmpty() && QFileInfo(arguments.first()).canonicalFilePath() == QFileInfo(path).canonicalFilePath()))
            throw std::invalid_argument("A theme cannot follow or import its own output file");
        theme = action == "follow" ? followPalette(arguments.first()) : importPalette(arguments.first());
    } else if (action == "unfollow") {
        count(0);
        theme.source.clear();
    } else if (action == "export") {
        count(1);
        if (!options.dryRun)
            writeJson(absolutePath(arguments.first()), theme.toJson());
        else
            printJson(theme.toJson());
        return 0;
    } else {
        throw std::invalid_argument(("Unknown theme command: " + action).toStdString());
    }
    if (options.dryRun)
        printJson(theme.toJson());
    else {
        writeJson(path, theme.toJson());
        QTextStream(stdout) << path << '\n';
    }
    return 0;
}

int cameraCommand(const Options &options) {
    const auto action = options.arguments.first();
    if (action != "devices" && action != "controls" && action != "set")
        throw std::invalid_argument(("Unknown command: " + action).toStdString());
    if (options.arguments.size() != (action == "set" ? 3 : 1))
        throw std::invalid_argument("Incorrect camera arguments; see --help");
    CameraService service;
    bool writing = false;
    QObject::connect(&service, &CameraService::error, &service, [](const QString &message) {
        QTextStream(stderr) << "webcam-controls: " << message << '\n';
        QCoreApplication::exit(1);
    });
    QObject::connect(&service, &CameraService::writeFinished, &service, [&options, &service](const QString &name, const QString &value, bool success) {
        if (success) {
            if (options.dryRun)
                printJson({{"program", "v4l2-ctl"}, {"arguments", QJsonArray{"--device=" + service.device(), "--set-ctrl=" + name + '=' + value}}});
            else
                printJson({{"device", service.device()}, {"control", name}, {"value", value}});
        }
        QCoreApplication::exit(success ? 0 : 1);
    });
    QObject::connect(&service, &CameraService::stateLoaded, &service, [&service, &options, &writing, action] {
        if (writing)
            return;
        if (action == "controls") {
            QJsonArray controls;
            for (const auto &control : service.controls())
                controls.append(control.toJson());
            printJson({{"device", service.device()}, {"controls", controls}});
            QCoreApplication::exit(0);
        } else {
            writing = true;
            service.setControl(options.arguments[1], options.arguments[2], options.dryRun);
        }
    });
    QObject::connect(&service, &CameraService::devicesChanged, &service, [&service, &options, action] {
        if (action == "devices") {
            QJsonArray devices;
            for (const auto &device : service.devices())
                devices.append(QJsonObject{{"path", device.path}, {"name", device.name}, {"detail", device.detail}});
            printJson({{"devices", devices}});
            QCoreApplication::exit(0);
            return;
        }
        QString device = options.window.device;
        if (device.isEmpty() && !service.devices().isEmpty())
            device = service.devices().first().path;
        const bool available = std::any_of(service.devices().begin(), service.devices().end(), [&device](const Device &candidate) { return candidate.path == device; });
        if (!available) {
            QTextStream(stderr) << "webcam-controls: No matching V4L2 capture device is available\n";
            QCoreApplication::exit(2);
            return;
        }
        service.selectDevice(device);
    });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &service, [] {
        QTextStream(stderr) << "webcam-controls: Camera operation did not complete\n";
        QCoreApplication::exit(1);
    });
    watchdog.start(30000);
    QTimer::singleShot(0, &service, &CameraService::discover);
    return QCoreApplication::exec();
}
}

Options parseOptions(const QStringList &arguments) {
    Options options;
    options.window.configDirectory = defaultConfigDirectory();
    for (int index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        if (argument == "--help" || argument == "-h")
            options.help = true;
        else if (argument == "--version")
            options.version = true;
        else if (argument == "--dry-run")
            options.dryRun = true;
        else if (argument == "--no-preview")
            options.window.preview = false;
        else if (argument == "--config-dir" || argument == "--device" || argument == "--preview-width" || argument == "--preview-fps") {
            if (++index >= arguments.size())
                throw std::invalid_argument(("Missing value for " + argument).toStdString());
            const auto value = arguments[index];
            if (argument == "--config-dir") {
                if (value.isEmpty())
                    throw std::invalid_argument("Configuration directory must not be empty");
                options.window.configDirectory = absolutePath(value);
            } else if (argument == "--device") {
                if (!validDevice(value))
                    throw std::invalid_argument("Device must have the form /dev/videoN");
                options.window.device = value;
            } else {
                bool valid = false;
                const auto number = value.toInt(&valid);
                const bool width = argument == "--preview-width";
                if (!valid || number < (width ? 160 : 1) || number > (width ? 3840 : 60))
                    throw std::invalid_argument("Preview width must be 160–3840; frame rate must be 1–60");
                if (width)
                    options.window.previewWidth = number;
                else
                    options.window.previewFps = number;
            }
        } else if (argument.startsWith("--")) {
            throw std::invalid_argument(("Unknown option: " + argument).toStdString());
        } else {
            options.arguments.append(argument);
        }
    }
    if (options.arguments.isEmpty() && options.dryRun && !options.help)
        throw std::invalid_argument("--dry-run requires a theme or camera command");
    return options;
}

void printHelp() {
    QTextStream(stdout) << R"(Webcam Controls — a standalone V4L2 camera client

Usage:
  webcam-controls [--device /dev/videoN] [--no-preview]
  webcam-controls devices
  webcam-controls controls [--device /dev/videoN]
  webcam-controls set CONTROL VALUE [--device /dev/videoN] [--dry-run]
  webcam-controls theme list|path|show
  webcam-controls theme preset NAME [--dry-run]
  webcam-controls theme set COLOR=#RRGGBB ... [--dry-run]
  webcam-controls theme import FILE_OR_DIRECTORY [--dry-run]
  webcam-controls theme follow FILE_OR_DIRECTORY [--dry-run]
  webcam-controls theme unfollow [--dry-run]
  webcam-controls theme export FILE [--dry-run]

Options:
  --config-dir DIRECTORY   Override the XDG configuration directory
  --preview-width PIXELS   Preferred maximum capture width (default 640)
  --preview-fps FPS        Preferred maximum frame rate (default 30)
  --no-preview             Keep the camera available to other applications
  --dry-run                Validate and print changes without writing them
  --help                   Show this help
  --version                Show the version

Theme colors: background, surface, surface_alt, text, muted, accent,
              accent_text, border, danger

Theme writes replace theme.json atomically; open windows reload immediately.
Following a palette also follows file replacement and directory symlink changes.
Camera commands require v4l2-ctl from v4l-utils. Preview uses Qt Multimedia.
)";
}

int runCommand(const Options &options) {
    return options.arguments.first() == "theme" ? themeCommand(options) : cameraCommand(options);
}
}
