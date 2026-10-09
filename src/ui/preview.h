#pragma once

#include "core/theme.h"
#include <QElapsedTimer>
#include <QImage>
#include <QWidget>
#include <memory>

class QCamera;
class QMediaCaptureSession;
class QMediaDevices;
class QVideoSink;

namespace webcam {
class Preview : public QWidget {
    Q_OBJECT
public:
    explicit Preview(int maximumWidth, int maximumFps, QWidget *parent = nullptr);
    ~Preview() override;
    void setDevice(const QString &path);
    void setPreviewEnabled(bool enabled);
    void setSuspended(bool suspended);
    void setFrozen(bool frozen) { frozen_ = frozen; }
    void applyTheme(const Theme &theme);
    bool previewEnabled() const { return enabled_; }
signals:
    void statusChanged(const QString &status);
    void devicesChanged();
    void streamingChanged(bool active);
protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    QString path_;
    QString message_ = "Connect a camera to get started";
    bool enabled_ = true;
    bool suspended_ = false;
    bool frozen_ = false;
    bool wasLive_ = false;
    int maximumWidth_;
    int maximumFps_;
    Theme theme_;
    QImage frame_;
    QElapsedTimer frameClock_;
    std::unique_ptr<QMediaDevices> devices_;
    std::unique_ptr<QCamera> camera_;
    std::unique_ptr<QMediaCaptureSession> session_;
    std::unique_ptr<QVideoSink> sink_;
    void start();
    void stop();
    void status(const QString &text);
};
}
