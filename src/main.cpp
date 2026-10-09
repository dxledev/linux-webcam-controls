#include "cli.h"
#include "core/storage.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication::setApplicationName("webcam-controls");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("WebcamControls");
    try {
        QStringList arguments;
        for (int index = 1; index < argc; ++index)
            arguments.append(QString::fromLocal8Bit(argv[index]));
        const auto options = webcam::parseOptions(arguments);
        if (options.help || options.version || !options.arguments.isEmpty()) {
            QCoreApplication application(argc, argv);
            if (options.help) {
                webcam::printHelp();
                return 0;
            }
            if (options.version) {
                QTextStream(stdout) << "Webcam Controls 1.0.0\n";
                return 0;
            }
            return webcam::runCommand(options);
        }
        if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "wayland");
        if (qEnvironmentVariableIsEmpty("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION"))
            qputenv("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION", "shm");
        QApplication application(argc, argv);
        application.setApplicationDisplayName("Webcam Controls");
        application.setDesktopFileName("io.github.webcam_controls");
        application.setStyle("Fusion");
        auto theme = webcam::presetTheme("slate");
        try {
            theme = webcam::ensureTheme(options.window.configDirectory + "/theme.json");
        } catch (const std::exception &error) {
            qWarning("Cannot load saved theme: %s", error.what());
        }
        webcam::Window window(options.window, theme);
        window.show();
        return application.exec();
    } catch (const std::exception &error) {
        QTextStream(stderr) << "webcam-controls: " << error.what() << '\n';
        return 1;
    }
}
