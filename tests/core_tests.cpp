#include "core/camera_service.h"
#include "core/storage.h"
#include "core/theme_source.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace webcam;

class CoreTests : public QObject {
    Q_OBJECT
private:
    QString fixtures() const { return QString(TEST_SOURCE_DIRECTORY) + "/fixtures"; }
    QString fixture(const QString &name) const {
        QFile file(fixtures() + '/' + name);
        if (!file.open(QIODevice::ReadOnly))
            qFatal("Cannot read fixture");
        return QString::fromUtf8(file.readAll());
    }
private slots:
    void themeValidation() {
        const auto theme = presetTheme("slate");
        QCOMPARE(parseTheme(theme.toJson()), theme);
        QCOMPARE(setColors(theme, {"accent=#abcdef"}).hex("accent"), "#abcdef");
        QVERIFY_EXCEPTION_THROWN(setColors(theme, {"accent=red"}), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(setColors(theme, {"typo=#ffffff"}), std::invalid_argument);
        auto incomplete = theme.toJson();
        incomplete.insert("colors", QJsonObject{});
        QVERIFY_EXCEPTION_THROWN(parseTheme(incomplete), std::invalid_argument);
    }

    void paletteFormats() {
        QTemporaryDir temporary;
        const auto json = temporary.path() + "/colors.json";
        writeJson(json, {{"colours", QJsonObject{{"background", "#102030"}, {"onSurface", "#eeeeee"}, {"primary", "#c0ffee"}}}});
        auto theme = followPalette(json);
        QCOMPARE(theme.hex("accent"), "#c0ffee");
        QCOMPARE(theme.source, json);
        writeJson(json, {{"mSurface", "#223344"}, {"mOnSurface", "#ffffff"}, {"mPrimary", "#aa33cc"}});
        QCOMPARE(importPalette(json).hex("background"), "#223344");
        writeJson(json, {{"mode", "light"}, {"light", QJsonObject{{"background", "#ffffff"}, {"foreground", "#111111"}, {"primary", "#123456"}}}});
        QCOMPARE(importPalette(json).hex("accent"), "#123456");
        const auto qml = temporary.path() + "/Colors.qml";
        QFile file(qml);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("readonly property color background: Qt.rgba(16 / 255, 32 / 255, 48 / 255, 1)\nproperty color foreground: \"#eeeeee\"\nproperty color primary: \"#c0ffee\"\n");
        file.close();
        QCOMPARE(importPalette(qml).hex("background"), "#102030");
        QCOMPARE(importPalette(temporary.path()).hex("accent"), "#c0ffee");
    }

    void controlTypesAndFlags() {
        const auto controls = parseControls(fixture("controls.txt"));
        QCOMPARE(controls.size(), 16);
        auto control = [&controls](const QString &name) {
            return *std::find_if(controls.begin(), controls.end(), [&name](const Control &value) { return value.name == name; });
        };
        QCOMPARE(control("brightness").kind(), ControlKind::Slider);
        QCOMPARE(control("white_balance_automatic").kind(), ControlKind::Toggle);
        QCOMPARE(control("auto_exposure").kind(), ControlKind::Menu);
        QCOMPARE(control("auto_exposure").options.size(), 2);
        QCOMPARE(control("auto_exposure").valueLabel(), "Aperture Priority Mode");
        QVERIFY(control("exposure_time_absolute").blocked());
        QVERIFY(!control("focus_absolute").canReset());
        QCOMPARE(control("sharpness").kind(), ControlKind::ReadOnly);
        QCOMPARE(control("trigger_now").kind(), ControlKind::Action);
        QCOMPARE(control("frame_ticks").value, "9223372036854775806");
        QCOMPARE(control("frame_ticks").kind(), ControlKind::Value);
        QCOMPARE(control("device_name").value, "Office camera");
        QVERIFY(control("device_name").writable());
        QCOMPARE(control("sensor_table").kind(), ControlKind::ReadOnly);
    }

    void validateDriverConstraints() {
        auto controls = parseControls(fixture("controls.txt"));
        const auto zoom = *std::find_if(controls.begin(), controls.end(), [](const Control &control) { return control.name == "zoom_absolute"; });
        QCOMPARE(validateValue(zoom, "0x69"), "105");
        QVERIFY_EXCEPTION_THROWN(validateValue(zoom, "106"), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(validateValue(zoom, "99"), std::invalid_argument);
        QVERIFY_EXCEPTION_THROWN(validateValue(zoom, "105,brightness=0"), std::invalid_argument);
        const auto menu = *std::find_if(controls.begin(), controls.end(), [](const Control &control) { return control.name == "auto_exposure"; });
        QVERIFY_EXCEPTION_THROWN(validateValue(menu, "2"), std::invalid_argument);
        QVERIFY(!validDevice("/tmp/video0"));
        QVERIFY(!validDevice("/dev/video0;id"));
        QVERIFY(validDevice("/dev/video12"));
    }

    void metadataFiltering() {
        QVERIFY(!isCaptureDevice("Capabilities : 0x1\n Video Capture\nDevice Caps : 0x1\n Metadata Capture\n"));
        QVERIFY(isCaptureDevice("Capabilities : 0x1\n Video Capture\nDevice Caps : 0x1\n Video Capture Multiplanar\n"));
        QVERIFY(isCaptureDevice("Capabilities : 0x1\n Video Capture\n"));
    }

    void cameraCommands() {
        QTemporaryDir temporary;
        const auto log = temporary.path() + "/commands.log";
        qputenv("WEBCAM_CONTROLS_TEST_LOG", log.toUtf8());
        CameraService service(fixtures() + "/v4l2-ctl");
        QSignalSpy discovered(&service, &CameraService::devicesChanged);
        QSignalSpy loaded(&service, &CameraService::stateLoaded);
        QSignalSpy writes(&service, &CameraService::writeFinished);
        QSignalSpy errors(&service, &CameraService::error);
        service.discover();
        QTRY_COMPARE(discovered.count(), 1);
        QCOMPARE(service.devices().size(), 2);
        QCOMPARE(service.devices().last().path, "/dev/video2");
        service.selectDevice("/dev/video0");
        QTRY_COMPARE(loaded.count(), 1);
        service.setControl("brightness", "18");
        QTRY_COMPARE(writes.count(), 1);
        QTRY_COMPARE(loaded.count(), 2);
        service.setControl("exposure_time_absolute", "50");
        QCOMPARE(writes.count(), 2);
        QCOMPARE(errors.count(), 1);
        service.setControl("zoom_absolute", "106");
        QCOMPARE(writes.count(), 3);
        service.setControl("device_name", "Studio Camera");
        QTRY_COMPARE(writes.count(), 4);
        QTRY_VERIFY(!service.busy());
        service.setControl("contrast", "99");
        QTRY_COMPARE(writes.count(), 5);
        QVERIFY(!writes.last()[2].toBool());
        QTRY_VERIFY(!service.busy());
        QFile file(log);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto commands = file.readAll();
        QVERIFY(commands.contains("--device=/dev/video0 --set-ctrl=brightness=18"));
        QVERIFY(commands.contains("--set-ctrl=device_name=Studio Camera"));
        QVERIFY(!commands.contains("--set-ctrl=exposure_time_absolute"));
        QVERIFY(!commands.contains("--set-ctrl=zoom_absolute"));
        qunsetenv("WEBCAM_CONTROLS_TEST_LOG");
    }
};

QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
