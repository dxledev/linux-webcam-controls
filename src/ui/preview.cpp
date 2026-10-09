#include "preview.h"
#include "icons.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPainter>
#include <QPainterPath>
#include <QVideoFrame>
#include <QVideoSink>
#include <limits>

namespace webcam {
Preview::Preview(int maximumWidth, int maximumFps, QWidget *parent)
    : QWidget(parent), maximumWidth_(maximumWidth), maximumFps_(maximumFps), theme_(presetTheme("slate")) {
    setObjectName("preview");
    setMinimumSize(280, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

Preview::~Preview() { stop(); }

void Preview::applyTheme(const Theme &theme) { theme_ = theme; update(); }

void Preview::status(const QString &text) { message_ = text; emit statusChanged(text); update(); }

void Preview::setDevice(const QString &path) {
    if (path_ == path)
        return;
    stop();
    path_ = path;
    if (path_.isEmpty())
        status("Connect a camera to get started");
    else
        start();
}

void Preview::setPreviewEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled)
        start();
    else {
        stop();
        status("Preview paused");
    }
}

void Preview::setSuspended(bool suspended) {
    suspended_ = suspended;
    if (suspended)
        stop();
    else
        start();
}

void Preview::start() {
    if (!enabled_ || suspended_ || !isVisible() || path_.isEmpty() || camera_)
        return;
    if (!devices_) {
        devices_ = std::make_unique<QMediaDevices>();
        connect(devices_.get(), &QMediaDevices::videoInputsChanged, this, [this] { emit devicesChanged(); });
    }
    QCameraDevice device;
    for (const auto &candidate : QMediaDevices::videoInputs())
        if (QString::fromUtf8(candidate.id()) == path_) {
            device = candidate;
            break;
        }
    if (device.isNull()) {
        status("Preview is unavailable for this capture device");
        return;
    }
    camera_ = std::make_unique<QCamera>(device);
    connect(camera_.get(), &QCamera::activeChanged, this, &Preview::streamingChanged);
    QCameraFormat format;
    qint64 bestScore = std::numeric_limits<qint64>::max();
    for (const auto &candidate : device.videoFormats()) {
        const auto size = candidate.resolution();
        const qint64 score = qAbs(size.width() - maximumWidth_) * 1000LL
                             + (size.width() > maximumWidth_ ? 1000000LL : 0LL)
                             + (candidate.maxFrameRate() > maximumFps_ ? 10000000LL : 0LL)
                             - size.height();
        if (score < bestScore) {
            bestScore = score;
            format = candidate;
        }
    }
    if (!format.isNull())
        camera_->setCameraFormat(format);
    sink_ = std::make_unique<QVideoSink>();
    session_ = std::make_unique<QMediaCaptureSession>();
    session_->setCamera(camera_.get());
    session_->setVideoSink(sink_.get());
    connect(camera_.get(), &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &error) {
        frame_ = {};
        wasLive_ = false;
        status(error.isEmpty() ? "Camera is unavailable or in use by another app" : error);
    });
    connect(sink_.get(), &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (frozen_)
            return;
        if (frameClock_.isValid() && frameClock_.elapsed() < 1000 / maximumFps_)
            return;
        auto image = frame.toImage();
        if (image.isNull())
            return;
        frameClock_.restart();
        frame_ = std::move(image);
        if (!wasLive_) {
            wasLive_ = true;
            emit statusChanged(QString("Live · %1 × %2").arg(frame_.width()).arg(frame_.height()));
        }
        update();
    });
    status("Starting preview…");
    camera_->start();
}

void Preview::stop() {
    if (camera_)
        camera_->stop();
    session_.reset();
    sink_.reset();
    camera_.reset();
    frame_ = {};
    wasLive_ = false;
    frameClock_.invalidate();
    update();
}

void Preview::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(rect()), 12, 12);
    painter.setClipPath(clip);
    painter.fillRect(rect(), theme_.color("background"));
    if (!frame_.isNull()) {
        auto size = frame_.size();
        size.scale(this->size(), Qt::KeepAspectRatio);
        const QRect target((width() - size.width()) / 2, (height() - size.height()) / 2, size.width(), size.height());
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, frame_);
        return;
    }
    const QRect icon(width() / 2 - 28, height() / 2 - 66, 56, 56);
    themedIcon(Icon::Camera, theme_.color("muted"), devicePixelRatioF(), 56).paint(&painter, icon);
    painter.setPen(theme_.color("text"));
    auto font = painter.font();
    font.setPixelSize(17);
    painter.setFont(font);
    painter.drawText(QRect(24, height() / 2 + 2, width() - 48, 70), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, message_);
}

void Preview::showEvent(QShowEvent *) { start(); }
void Preview::hideEvent(QHideEvent *) { stop(); }
}
