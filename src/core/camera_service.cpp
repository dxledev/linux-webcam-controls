#include "camera_service.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <stdexcept>

namespace webcam {
CameraService::CameraService(QString executable, QObject *parent) : QObject(parent) {
    if (executable.isEmpty())
        executable = qEnvironmentVariable("WEBCAM_CONTROLS_V4L2_CTL", "v4l2-ctl");
    executable_ = QFileInfo(executable).isAbsolute() ? executable : QStandardPaths::findExecutable(executable);
    timeout_.setSingleShot(true);
    timeout_.setInterval(5000);
    connect(&timeout_, &QTimer::timeout, this, [this] { timedOut_ = true; process_.kill(); });
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        finish(code == 0 && status == QProcess::NormalExit && !timedOut_);
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(false);
    });
}

CameraService::~CameraService() {
    process_.disconnect(this);
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(500);
    }
}

void CameraService::enqueue(Request request) {
    queue_.push_back(std::move(request));
    startNext();
}

void CameraService::startNext() {
    if (completed_ || queue_.empty())
        return;
    auto request = std::move(queue_.front());
    queue_.pop_front();
    completed_ = std::move(request.finished);
    timedOut_ = false;
    process_.setProcessEnvironment([] {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("LC_ALL", "C");
        return environment;
    }());
    process_.start(executable_.isEmpty() ? "v4l2-ctl" : executable_, request.arguments, QIODevice::ReadOnly);
    timeout_.start();
    emit busyChanged();
}

void CameraService::finish(bool success) {
    if (!completed_)
        return;
    timeout_.stop();
    const auto output = QString::fromUtf8(process_.readAllStandardOutput());
    auto message = QString::fromUtf8(process_.readAllStandardError()).trimmed();
    if (!success && message.isEmpty())
        message = timedOut_ ? "The camera command timed out" : process_.errorString();
    auto callback = std::move(completed_);
    completed_ = {};
    callback(success, output, message);
    startNext();
    emit busyChanged();
}

void CameraService::discover() {
    const auto revision = ++revision_;
    queue_.clear();
    enqueue({{"--list-devices"}, [this, revision](bool success, const QString &output, const QString &message) {
        if (revision != revision_)
            return;
        if (!success) {
            emit error("Cannot list cameras: " + message + ". Install v4l-utils if v4l2-ctl is missing.");
            return;
        }
        QStringList candidates;
        for (const auto &line : output.split('\n'))
            if (validDevice(line.trimmed()))
                candidates.append(line.trimmed());
        candidates.removeDuplicates();
        probeDevices(candidates, {}, revision);
    }});
}

void CameraService::probeDevices(QStringList candidates, QVector<Device> found, quint64 revision) {
    if (revision != revision_)
        return;
    if (candidates.isEmpty()) {
        devices_ = std::move(found);
        emit devicesChanged();
        return;
    }
    const auto path = candidates.takeFirst();
    enqueue({{"--device=" + path, "--info"}, [this, path, candidates, found, revision](bool success, const QString &output, const QString &) mutable {
        if (success && isCaptureDevice(output)) {
            auto field = [&output](const QString &name) {
                const QRegularExpression pattern("^\\s*" + name + "\\s*:\\s*(.*)$", QRegularExpression::MultilineOption);
                return pattern.match(output).captured(1).trimmed();
            };
            found.append({path, field("Card type").isEmpty() ? path : field("Card type"), field("Bus info")});
        }
        probeDevices(candidates, found, revision);
    }});
}

void CameraService::selectDevice(const QString &path) {
    if (!path.isEmpty() && !validDevice(path)) {
        emit error("Invalid V4L2 device: " + path);
        return;
    }
    ++revision_;
    queue_.clear();
    device_ = path;
    controls_.clear();
    emit controlsChanged();
    if (!path.isEmpty())
        refresh();
}

void CameraService::refresh() {
    if (device_.isEmpty())
        return;
    const auto revision = revision_;
    enqueue({{"--device=" + device_, "--list-ctrls-menus"}, [this, revision](bool success, const QString &output, const QString &message) {
        if (revision != revision_)
            return;
        if (!success) {
            emit error("Cannot read camera controls: " + message);
            return;
        }
        controls_ = parseControls(output);
        emit controlsChanged();
        emit stateLoaded();
    }});
}

void CameraService::setControl(const QString &name, const QString &value, bool dryRun) {
    try {
        const auto found = std::find_if(controls_.begin(), controls_.end(), [&name](const Control &control) { return control.name == name; });
        if (found == controls_.end())
            throw std::invalid_argument("Unknown camera control");
        const auto validated = validateValue(*found, value);
        if (dryRun) {
            emit writeFinished(name, validated, true);
            return;
        }
        const auto path = device_;
        const auto revision = revision_;
        enqueue({{"--device=" + path, "--set-ctrl=" + name + '=' + validated}, [this, name, validated, revision](bool success, const QString &, const QString &message) {
            if (revision != revision_)
                return;
            if (!success)
                emit error("Could not apply " + name + ": " + message);
            emit writeFinished(name, validated, success);
            if (queue_.empty())
                refresh();
        }});
    } catch (const std::exception &exception) {
        emit error(QString::fromUtf8(exception.what()));
        emit writeFinished(name, value, false);
    }
}

void CameraService::resetDefaults() {
    const auto controls = controls_;
    for (const auto &control : controls)
        if (control.canReset())
            setControl(control.name, control.defaultValue);
}
}
