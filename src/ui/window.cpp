#include "window.h"
#include "control_row.h"
#include "icons.h"
#include "modal.h"
#include "preview.h"
#include "core/storage.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace webcam {
namespace {
QLabel *label(const QString &text, const QString &name = {}) {
    auto *widget = new QLabel(text);
    widget->setObjectName(name);
    widget->setTextFormat(Qt::PlainText);
    return widget;
}

QPushButton *button(const QString &text, const QString &name = {}) {
    auto *widget = new QPushButton(text);
    widget->setObjectName(name);
    widget->setCursor(Qt::PointingHandCursor);
    return widget;
}
}

Window::Window(WindowOptions options, Theme theme, QWidget *parent)
    : QMainWindow(parent), options_(std::move(options)), service_(options_.executable, this),
      themeWatcher_(options_.configDirectory + "/theme.json", std::move(theme), this) {
    setWindowTitle("Webcam Controls");
    setObjectName("webcamControlsWindow");
    setMinimumSize(760, 600);
    resize(1120, 760);
    selectedDevice_ = options_.device;
    if (QFileInfo::exists(options_.configDirectory + "/settings.json")) {
        try {
            const auto settings = readJson(options_.configDirectory + "/settings.json");
            if (selectedDevice_.isEmpty())
                selectedDevice_ = settings.value("device").toString();
        } catch (const std::exception &error) {
            qWarning("Cannot restore camera selection: %s", error.what());
        }
    }
    buildInterface();
    applyTheme();
    connect(&themeWatcher_, &ThemeWatcher::changed, this, &Window::applyTheme);
    connect(&themeWatcher_, &ThemeWatcher::rejected, this, [this](const QString &message) {
        setStatus("Theme unchanged: " + message, true);
    });
    connect(&service_, &CameraService::devicesChanged, this, &Window::devicesChanged);
    connect(&service_, &CameraService::controlsChanged, this, &Window::rebuildControls);
    connect(&service_, &CameraService::error, this, [this](const QString &message) { setStatus(message, true); });
    connect(&service_, &CameraService::writeFinished, this, [this](const QString &name, const QString &, bool success) {
        if (success)
            setStatus("Applied " + QString(name).replace('_', ' ') + ".");
    });
    connect(&service_, &CameraService::busyChanged, this, [this] {
        refreshButton_->setEnabled(!service_.busy());
        deviceButton_->setEnabled(!service_.busy() && !service_.devices().isEmpty());
        resetButton_->setEnabled(!service_.busy() && std::any_of(service_.controls().begin(), service_.controls().end(), [](const Control &control) { return control.canReset(); }));
    });
    connect(preview_, &Preview::devicesChanged, this, [this] { if (!service_.busy()) service_.discover(); });
    streamRefresh_.setSingleShot(true);
    streamRefresh_.setInterval(80);
    connect(preview_, &Preview::streamingChanged, this, [this] { streamRefresh_.start(); });
    connect(&streamRefresh_, &QTimer::timeout, this, [this] {
        if (service_.busy())
            streamRefresh_.start();
        else
            service_.refresh();
    });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, modal_, &Modal::dismiss);
    auto *refresh = new QShortcut(QKeySequence::Refresh, this);
    connect(refresh, &QShortcut::activated, this, [this] { if (!modalVisible() && !service_.busy()) service_.discover(); });
    QTimer::singleShot(0, &service_, &CameraService::discover);
}

void Window::buildInterface() {
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    content_ = new QWidget(root);
    rootLayout->addWidget(content_);
    auto *layout = new QVBoxLayout(content_);
    layout->setContentsMargins(24, 24, 24, 18);
    layout->setSpacing(20);
    auto *header = new QHBoxLayout;
    header->setSpacing(14);
    brand_ = new QLabel;
    brand_->setFixedSize(44, 44);
    header->addWidget(brand_);
    auto *wordmark = new QVBoxLayout;
    wordmark->setSpacing(3);
    wordmark->addWidget(label("Webcam Controls", "wordmark"));
    deviceName_ = label("Camera preview & settings", "muted");
    wordmark->addWidget(deviceName_);
    header->addLayout(wordmark, 1);
    deviceButton_ = button("Choose camera", "deviceButton");
    deviceButton_->setMaximumWidth(260);
    deviceButton_->setEnabled(false);
    connect(deviceButton_, &QPushButton::clicked, this, &Window::showDevices);
    header->addWidget(deviceButton_);
    refreshButton_ = button({}, "iconButton");
    refreshButton_->setFixedSize(40, 40);
    refreshButton_->setAccessibleName("Refresh cameras and controls");
    connect(refreshButton_, &QPushButton::clicked, &service_, &CameraService::discover);
    header->addWidget(refreshButton_);
    layout->addLayout(header);
    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(18);
    splitter_->addWidget(buildPreviewCard());
    splitter_->addWidget(buildControlsCard());
    splitter_->setStretchFactor(0, 3);
    splitter_->setStretchFactor(1, 2);
    splitter_->setSizes({620, 410});
    layout->addWidget(splitter_, 1);
    status_ = label("Settings are applied directly to your camera.", "status");
    status_->setWordWrap(true);
    status_->setMinimumHeight(24);
    layout->addWidget(status_);
    modal_ = new Modal(root, content_);
    connect(modal_, &Modal::dismissed, this, [this] { preview_->setFrozen(false); });
}

QWidget *Window::buildPreviewCard() {
    auto *card = new QFrame;
    card->setObjectName("card");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    header->addWidget(label("CAMERA PREVIEW", "eyebrow"));
    header->addStretch();
    previewStatus_ = label("No camera", "smallMuted");
    header->addWidget(previewStatus_);
    layout->addLayout(header);
    preview_ = new Preview(options_.previewWidth, options_.previewFps);
    connect(preview_, &Preview::statusChanged, this, [this](const QString &status) {
        previewStatus_->setText(status.startsWith("Live") || status == "Preview paused" ? status
            : status == "Starting preview…" ? "Starting…" : service_.device().isEmpty() ? "No camera" : "Unavailable");
    });
    preview_->setPreviewEnabled(options_.preview);
    layout->addWidget(preview_, 1);
    auto *footer = new QHBoxLayout;
    auto *hint = label("Preview uses the selected camera", "smallMuted");
    hint->setWordWrap(true);
    footer->addWidget(hint, 1);
    previewButton_ = button(options_.preview ? "Pause preview" : "Start preview");
    connect(previewButton_, &QPushButton::clicked, this, [this] {
        preview_->setPreviewEnabled(!preview_->previewEnabled());
        applyTheme();
    });
    footer->addWidget(previewButton_);
    layout->addLayout(footer);
    return card;
}

QWidget *Window::buildControlsCard() {
    auto *card = new QFrame;
    card->setObjectName("card");
    card->setMinimumWidth(320);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 18, 12, 14);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    header->addWidget(label("Camera settings", "sectionTitle"), 1);
    resetButton_ = button("Reset defaults");
    resetButton_->setEnabled(false);
    connect(resetButton_, &QPushButton::clicked, this, &Window::showReset);
    header->addWidget(resetButton_);
    layout->addLayout(header);
    search_ = new QLineEdit;
    search_->setObjectName("search");
    search_->setPlaceholderText("Search settings…");
    search_->setAccessibleName("Search camera settings");
    search_->setContextMenuPolicy(Qt::NoContextMenu);
    connect(search_, &QLineEdit::textChanged, this, &Window::rebuildControls);
    layout->addWidget(search_);
    controlsScroll_ = new QScrollArea;
    controlsScroll_->setWidgetResizable(true);
    controlsScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(controlsScroll_, 1);
    return card;
}

void Window::applyTheme() {
    const auto &theme = themeWatcher_.theme();
    QString css = R"(
        QWidget { background: transparent; color: @text; font-family: "Noto Sans", sans-serif; font-size: 13px; }
        QMainWindow, QMainWindow > QWidget { background: @background; }
        QFrame#card, QFrame#modalCard { background: @surface; border: 1px solid @border; border-radius: 16px; }
        QLabel#wordmark { font-size: 24px; font-weight: 650; }
        QLabel#sectionTitle, QLabel#modalTitle { font-size: 17px; font-weight: 600; }
        QLabel#muted, QLabel#status { color: @muted; }
        QLabel#smallMuted { color: @muted; font-size: 11px; }
        QLabel#eyebrow { color: @muted; font-size: 10px; font-weight: 650; letter-spacing: 1px; }
        QLabel#groupTitle { color: @accent; font-size: 11px; font-weight: 650; padding-top: 10px; }
        QLabel#controlValue { color: @accent; }
        QLabel[error="true"] { color: @danger; }
        QPushButton { background: @surface_alt; border: 1px solid @border; border-radius: 9px; padding: 9px 12px; }
        QPushButton:hover { border-color: @accent; }
        QPushButton:focus, QLineEdit:focus { border-color: @accent; }
        QPushButton:pressed { background: @border; }
        QPushButton:disabled, QCheckBox:disabled { color: @muted; }
        QPushButton#primaryButton, QPushButton#selectedChoice { background: @accent; color: @accent_text; border-color: @accent; }
        QPushButton#choiceButton, QPushButton#selectedChoice { text-align: left; }
        QPushButton#iconButton, QPushButton#resetButton { background: transparent; padding: 3px; border-color: transparent; }
        QPushButton#iconButton:hover, QPushButton#resetButton:hover { background: @surface_alt; border-color: @border; }
        QLineEdit { background: @background; border: 1px solid @border; border-radius: 9px; padding: 10px; selection-background-color: @accent; selection-color: @accent_text; }
        QScrollArea { border: none; background: transparent; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }
        QScrollBar::handle:vertical { background: @border; min-height: 28px; border-radius: 3px; }
        QScrollBar::handle:vertical:hover { background: @muted; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QSlider::groove:horizontal { background: @surface_alt; height: 5px; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
        QSlider::handle:horizontal { background: @accent; border: 3px solid @surface; width: 14px; height: 14px; margin: -7px 0; border-radius: 9px; }
        QSlider::handle:horizontal:disabled, QSlider::sub-page:horizontal:disabled { background: @muted; }
        QCheckBox { spacing: 9px; }
        QCheckBox::indicator { width: 32px; height: 18px; border-radius: 9px; background: @surface_alt; border: 1px solid @border; }
        QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
        QCheckBox::indicator:disabled { background: @border; }
        QSplitter::handle { background: transparent; }
    )";
    // Replace longer names first because accent is a prefix of accent_text.
    auto keys = colorKeys();
    std::sort(keys.begin(), keys.end(), [](const QString &left, const QString &right) { return left.size() > right.size(); });
    for (const auto &key : keys)
        css.replace('@' + key, theme.hex(key));
    setStyleSheet(css);
    QPalette palette;
    palette.setColor(QPalette::Window, theme.color("background"));
    palette.setColor(QPalette::WindowText, theme.color("text"));
    palette.setColor(QPalette::Text, theme.color("text"));
    palette.setColor(QPalette::Base, theme.color("background"));
    palette.setColor(QPalette::Highlight, theme.color("accent"));
    palette.setColor(QPalette::HighlightedText, theme.color("accent_text"));
    setPalette(palette);
    const auto scale = devicePixelRatioF();
    const auto icon = themedIcon(Icon::Camera, theme.color("accent"), scale, 40);
    brand_->setPixmap(icon.pixmap(QSize(40, 40), scale));
    setWindowIcon(icon);
    deviceButton_->setIcon(themedIcon(Icon::Chevron, theme.color("muted"), scale));
    refreshButton_->setIcon(themedIcon(Icon::Refresh, theme.color("text"), scale));
    previewButton_->setText(preview_->previewEnabled() ? "Pause preview" : "Start preview");
    previewButton_->setIcon(themedIcon(preview_->previewEnabled() ? Icon::Pause : Icon::Play, theme.color("text"), scale));
    preview_->applyTheme(theme);
    for (auto *row : findChildren<ControlRow *>())
        row->applyTheme(theme);
    modal_->applyTheme(theme);
}

void Window::devicesChanged() {
    const auto &devices = service_.devices();
    if (devices.isEmpty()) {
        selectDevice({});
        setStatus("No capture camera detected. Connect a camera and refresh.");
        return;
    }
    const auto found = std::find_if(devices.begin(), devices.end(), [this](const Device &device) { return device.path == selectedDevice_; });
    selectDevice(found == devices.end() ? devices.first().path : found->path);
}

void Window::selectDevice(const QString &path) {
    if (modalVisible())
        modal_->dismiss();
    if (!path.isEmpty())
        selectedDevice_ = path;
    service_.selectDevice(path);
    preview_->setDevice(path);
    deviceButton_->setText(path.isEmpty() ? "Choose camera" : path);
    QString name = "Camera preview & settings";
    for (const auto &device : service_.devices())
        if (device.path == path)
            name = device.name;
    deviceName_->setText(name);
    saveSettings();
}

void Window::rebuildControls() {
    if (modalVisible())
        modal_->dismiss();
    const auto scroll = controlsScroll_->verticalScrollBar()->value();
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(3);
    QMap<QString, QVector<Control>> groups;
    const auto query = search_->text().trimmed();
    for (const auto &control : service_.controls())
        if (query.isEmpty() || control.label().contains(query, Qt::CaseInsensitive) || control.group.contains(query, Qt::CaseInsensitive))
            groups[control.group].append(control);
    QStringList ordered{"Exposure", "Image", "Color", "Focus", "Framing", "Device"};
    for (const auto &group : groups.keys())
        if (!ordered.contains(group))
            ordered.append(group);
    for (const auto &group : ordered) {
        if (!groups.contains(group))
            continue;
        layout->addWidget(label(group.toUpper(), "groupTitle"));
        for (const auto &control : groups[group]) {
            auto *row = new ControlRow(control);
            row->applyTheme(themeWatcher_.theme());
            connect(row, &ControlRow::writeRequested, &service_, [this](const QString &name, const QString &value) { service_.setControl(name, value); });
            connect(row, &ControlRow::menuRequested, this, &Window::showControlMenu);
            layout->addWidget(row);
        }
    }
    if (groups.isEmpty()) {
        auto *empty = label(service_.device().isEmpty() ? "Your camera settings will appear here." : (query.isEmpty() ? "No controls reported by this camera." : "No matching settings."), "muted");
        empty->setWordWrap(true);
        layout->addWidget(empty);
    }
    layout->addStretch();
    controlsScroll_->setWidget(body);
    controlsScroll_->verticalScrollBar()->setValue(scroll);
}

void Window::openModal() { preview_->setFrozen(true); }

void Window::showDevices() {
    QVector<QPair<QString, QString>> choices;
    for (const auto &device : service_.devices())
        choices.append({device.path, device.name + "  ·  " + device.path});
    openModal();
    modal_->choices("Choose camera", "Each entry is a capture device reported by your camera.", choices, service_.device(), [this](const QString &path) { selectDevice(path); });
}

void Window::showControlMenu(const Control &control) {
    QVector<QPair<QString, QString>> choices;
    for (auto option = control.options.begin(); option != control.options.end(); ++option)
        choices.append({QString::number(option.key()), option.value()});
    openModal();
    modal_->choices(control.label(), {}, choices, control.value, [this, name = control.name](const QString &value) { service_.setControl(name, value); });
}

void Window::showReset() {
    openModal();
    modal_->confirm("Reset camera settings?", "Restore the driver defaults for currently available controls. Controls locked by an automatic mode will keep their values.",
                    "Reset defaults", [this] { service_.resetDefaults(); });
}

bool Window::modalVisible() const { return modal_ && modal_->isVisible(); }

void Window::setStatus(const QString &text, bool error) {
    status_->setText(text);
    status_->setProperty("error", error);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);
}

void Window::saveSettings() {
    try {
        auto settings = QFileInfo::exists(options_.configDirectory + "/settings.json")
            ? readJson(options_.configDirectory + "/settings.json") : QJsonObject{};
        settings.insert("device", selectedDevice_);
        writeJson(options_.configDirectory + "/settings.json", settings);
    } catch (const std::exception &error) {
        setStatus("Could not save camera selection: " + QString::fromUtf8(error.what()), true);
    }
}

void Window::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (modal_)
        modal_->setGeometry(centralWidget()->rect());
}

void Window::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && preview_)
        preview_->setSuspended(isMinimized());
}

void Window::closeEvent(QCloseEvent *event) {
    preview_->setSuspended(true);
    saveSettings();
    QMainWindow::closeEvent(event);
}
}
