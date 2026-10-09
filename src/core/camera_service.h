#pragma once

#include "controls.h"
#include <QProcess>
#include <QTimer>
#include <deque>
#include <functional>

namespace webcam {
struct Device {
    QString path;
    QString name;
    QString detail;
    bool operator==(const Device &) const = default;
};

class CameraService : public QObject {
    Q_OBJECT
public:
    explicit CameraService(QString executable = {}, QObject *parent = nullptr);
    ~CameraService() override;
    void discover();
    void selectDevice(const QString &path);
    void refresh();
    void setControl(const QString &name, const QString &value, bool dryRun = false);
    void resetDefaults();
    const QVector<Device> &devices() const { return devices_; }
    const QVector<Control> &controls() const { return controls_; }
    QString device() const { return device_; }
    bool busy() const { return process_.state() != QProcess::NotRunning || !queue_.empty(); }

signals:
    void devicesChanged();
    void controlsChanged();
    void stateLoaded();
    void error(const QString &message);
    void busyChanged();
    void writeFinished(const QString &name, const QString &value, bool success);

private:
    struct Request {
        QStringList arguments;
        std::function<void(bool, QString, QString)> finished;
    };
    QString executable_;
    QString device_;
    QVector<Device> devices_;
    QVector<Control> controls_;
    QProcess process_;
    QTimer timeout_;
    std::deque<Request> queue_;
    std::function<void(bool, QString, QString)> completed_;
    quint64 revision_ = 0;
    bool timedOut_ = false;
    void enqueue(Request request);
    void startNext();
    void finish(bool success);
    void probeDevices(QStringList candidates, QVector<Device> found, quint64 revision);
};
}
