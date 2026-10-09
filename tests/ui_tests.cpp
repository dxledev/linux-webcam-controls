#include "core/storage.h"
#include "core/theme_source.h"
#include "ui/control_row.h"
#include "ui/modal.h"
#include "ui/theme_watcher.h"
#include "ui/window.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>

using namespace webcam;

class UiTests : public QObject {
    Q_OBJECT
private:
    QString fixtureTool() const { return QString(TEST_SOURCE_DIRECTORY) + "/fixtures/v4l2-ctl"; }
    void palette(const QString &directory, const QString &accent) {
        writeJson(directory + "/colors.json", {{"background", "#131820"}, {"foreground", "#eeeeee"}, {"primary", accent}});
    }
    void retarget(const QString &link, const QString &target) {
        const auto staged = link + ".next";
        QVERIFY(QFile::link(target, staged));
        QCOMPARE(std::rename(QFile::encodeName(staged).constData(), QFile::encodeName(link).constData()), 0);
    }
    void screenshot(QWidget &widget, const QString &name) {
        const auto directory = qEnvironmentVariable("WEBCAM_CONTROLS_SCREENSHOTS");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(widget.grab().save(directory + '/' + name + ".png"));
        }
    }
private slots:
    void selectorTransitions() {
        QTemporaryDir temporary;
        const auto themes = temporary.path() + "/themes";
        palette(themes + "/named-one", "#ffaaaa");
        palette(themes + "/named-two", "#aaffaa");
        palette(themes + "/dynamic-one", "#aaaaff");
        palette(themes + "/dynamic-two", "#ffaaff");
        const auto current = themes + "/current";
        const auto selector = themes + "/active";
        QVERIFY(QFile::link(themes + "/named-one", current));
        QVERIFY(QFile::link(current, selector));
        const auto output = temporary.path() + "/config/theme.json";
        auto theme = followPalette(selector);
        writeJson(output, theme.toJson());
        ThemeWatcher watcher(output, theme);
        QSignalSpy changed(&watcher, &ThemeWatcher::changed);
        QTest::qWait(100);
        retarget(current, themes + "/named-two");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#aaffaa");
        retarget(selector, themes + "/dynamic-one");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#aaaaff");
        palette(themes + "/dynamic-one", "#abcdef");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#abcdef");
        retarget(selector, themes + "/dynamic-two");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#ffaaff");
        retarget(selector, current);
        QTRY_COMPARE(watcher.theme().hex("accent"), "#aaffaa");
        retarget(selector, themes + "/dynamic-two");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#ffaaff");
        QCOMPARE(loadTheme(output), watcher.theme());
        QCOMPARE(watcher.theme().source, selector);
        QVERIFY(changed.count() >= 6);
        const auto stable = changed.count();
        QTest::qWait(250);
        QCOMPARE(changed.count(), stable);
    }

    void rejectedThemeAndRecovery() {
        QTemporaryDir temporary;
        const auto path = temporary.path() + "/theme.json";
        auto theme = presetTheme("slate");
        writeJson(path, theme.toJson());
        ThemeWatcher watcher(path, theme);
        QSignalSpy rejected(&watcher, &ThemeWatcher::rejected);
        writeJson(path, {{"invalid", true}});
        QTRY_VERIFY(!rejected.isEmpty());
        QCOMPARE(watcher.theme(), theme);
        writeJson(path, presetTheme("paper").toJson());
        QTRY_COMPARE(watcher.theme().name, "paper");
        theme = setColors(presetTheme("paper"), {"accent=#112233"});
        writeJson(path, theme.toJson());
        QTRY_COMPARE(watcher.theme().hex("accent"), "#112233");
    }

    void existingThemeCoordinator() {
        const auto coordinator = qEnvironmentVariable("WEBCAM_CONTROLS_THEME_COORDINATOR");
        if (coordinator.isEmpty())
            QSKIP("Optional check against an existing theme coordinator");
        QTemporaryDir temporary;
        const auto themes = temporary.path() + "/themes";
        palette(themes + "/named", "#aabbcc");
        palette(themes + "/other", "#ffaabb");
        palette(themes + "/.dynamic/caelestia", "#aaffaa");
        palette(themes + "/.dynamic/noctalia", "#aaaaff");
        for (const auto &provider : {"caelestia", "noctalia"}) {
            QFile marker(themes + "/.dynamic/" + provider + "/." + provider + "-generated");
            QVERIFY(marker.open(QIODevice::WriteOnly));
            marker.close();
            const auto directory = themes + "/.dynamic/" + provider + "/spicetify";
            QVERIFY(QDir().mkpath(directory));
            for (const auto &name : {"color.ini", "user.css"}) {
                QFile file(directory + '/' + name);
                QVERIFY(file.open(QIODevice::WriteOnly));
                file.close();
            }
        }
        QVERIFY(QFile::link(themes + "/named", themes + "/current"));
        const auto selector = themes + "/.caelestia-use";
        QVERIFY(QFile::link("current", selector));
        const auto output = temporary.path() + "/config/webcam-controls/theme.json";
        const auto theme = followPalette(selector);
        writeJson(output, theme.toJson());
        ThemeWatcher watcher(output, theme);
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("THEMES_DIR", themes);
        environment.insert("APPLICATION_THEME_LINK", selector);
        environment.insert("SPICETIFY_DYNAMIC_LINK", temporary.path() + "/config/spicetify/active");
        environment.insert("DYNAMIC_THEME_DIR", themes + "/.dynamic/caelestia");
        environment.insert("NOCTALIA_DYNAMIC_THEME_DIR", themes + "/.dynamic/noctalia");
        environment.insert("XDG_CONFIG_HOME", temporary.path() + "/config");
        auto activate = [&environment, &coordinator](const QStringList &arguments) {
            QProcess process;
            process.setProcessEnvironment(environment);
            process.start("/usr/bin/bash", QStringList{coordinator, "--activate-only"} + arguments);
            if (!process.waitForFinished(5000))
                return false;
            if (process.exitCode() != 0)
                qWarning().noquote() << process.readAllStandardError();
            return process.exitCode() == 0;
        };
        const auto themeCommand = qEnvironmentVariable("WEBCAM_CONTROLS_THEME_COMMAND");
        if (!themeCommand.isEmpty()) {
            environment.insert("THEME_APPLY_SCRIPT", coordinator);
            QProcess process;
            process.setProcessEnvironment(environment);
            process.start("/usr/bin/bash", {themeCommand, "--current-only", "other"});
            QVERIFY(process.waitForFinished(5000));
            QCOMPARE(process.exitCode(), 0);
            QTRY_COMPARE(watcher.theme().hex("accent"), "#ffaabb");
        }
        const QVector<QPair<QStringList, QString>> transitions{
            {{"dynamic", "caelestia"}, "#aaffaa"},
            {{"dynamic", "noctalia"}, "#aaaaff"},
            {{"dynamic", "caelestia"}, "#aaffaa"},
            {{"system"}, themeCommand.isEmpty() ? "#aabbcc" : "#ffaabb"},
            {{"dynamic", "noctalia"}, "#aaaaff"},
            {{"system"}, themeCommand.isEmpty() ? "#aabbcc" : "#ffaabb"},
            {{"dynamic", "caelestia"}, "#aaffaa"},
        };
        for (const auto &transition : transitions) {
            QVERIFY(activate(transition.first));
            QTRY_COMPARE(watcher.theme().hex("accent"), transition.second);
            QCOMPARE(loadTheme(output).hex("accent"), transition.second);
        }
        palette(themes + "/.dynamic/caelestia", "#123abc");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#123abc");
        QVERIFY(activate({"dynamic", "noctalia"}));
        QTRY_COMPARE(watcher.theme().hex("accent"), "#aaaaff");
        palette(themes + "/.dynamic/noctalia", "#c0ffee");
        QTRY_COMPARE(watcher.theme().hex("accent"), "#c0ffee");
        QCOMPARE(loadTheme(output).hex("accent"), "#c0ffee");
    }

    void windowAndEmbeddedMenus() {
        QTemporaryDir temporary;
        auto theme = presetTheme("slate");
        writeJson(temporary.path() + "/theme.json", theme.toJson());
        WindowOptions options;
        options.configDirectory = temporary.path();
        options.executable = fixtureTool();
        options.preview = false;
        Window window(options, theme);
        window.show();
        QTRY_COMPARE(window.findChildren<ControlRow *>().size(), 16);
        QCOMPARE(window.windowTitle(), "Webcam Controls");
        screenshot(window, "window");
        auto *picker = window.findChild<QPushButton *>("deviceButton");
        QTRY_VERIFY(picker->isEnabled());
        QTest::mouseClick(picker, Qt::LeftButton);
        QTRY_VERIFY(window.modalVisible());
        auto *modal = window.findChild<Modal *>();
        QVERIFY(modal && !modal->isWindow());
        auto *card = modal->findChild<QFrame *>("modalCard");
        QVERIFY(qAbs(card->geometry().center().x() - modal->rect().center().x()) <= 1);
        QVERIFY(qAbs(card->geometry().center().y() - modal->rect().center().y()) <= 1);
        screenshot(window, "device-modal");
        window.resize(820, 630);
        QTest::qWait(40);
        QCOMPARE(modal->size(), window.centralWidget()->size());
        QVERIFY(card->geometry().right() <= modal->width());
        writeJson(temporary.path() + "/theme.json", presetTheme("paper").toJson());
        QTRY_VERIFY(window.styleSheet().contains("#f0efe9"));
        QVERIFY(window.modalVisible());
        screenshot(window, "light-modal");
        QTest::keyClick(modal, Qt::Key_Escape);
        QVERIFY(!window.modalVisible());
        QTest::mouseClick(picker, Qt::LeftButton);
        QVERIFY(window.modalVisible());
        for (int index = 0; index < 6; ++index) {
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
            QVERIFY(modal->isAncestorOf(QApplication::focusWidget()));
        }
        modal->dismiss();
        auto *search = window.findChild<QLineEdit *>("search");
        search->setText("exposure");
        QCOMPARE(window.findChildren<ControlRow *>().size(), 2);
        search->clear();
        QCOMPARE(window.findChildren<ControlRow *>().size(), 16);
        ControlRow *menuRow = nullptr;
        for (auto *row : window.findChildren<ControlRow *>())
            if (row->control().name == "auto_exposure")
                menuRow = row;
        QVERIFY(menuRow);
        QTest::mouseClick(menuRow->findChild<QPushButton *>("choiceButton"), Qt::LeftButton);
        QVERIFY(window.modalVisible());
        QTest::mouseClick(modal, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
        QVERIFY(!window.modalVisible());
        int visibleWindows = 0;
        for (auto *top : QApplication::topLevelWidgets())
            if (top->isVisible())
                ++visibleWindows;
        QCOMPARE(visibleWindows, 1);
        QVERIFY(window.minimumSize().width() < window.maximumSize().width());
        window.close();
    }
};

QTEST_MAIN(UiTests)
#include "ui_tests.moc"
