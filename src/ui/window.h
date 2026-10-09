#pragma once

#include "core/camera_service.h"
#include "theme_watcher.h"
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QScrollArea;
class QSplitter;

namespace webcam {
class Preview;
class Modal;
struct WindowOptions {
    QString configDirectory;
    QString device;
    QString executable;
    bool preview = true;
    int previewWidth = 640;
    int previewFps = 30;
};

class Window : public QMainWindow {
    Q_OBJECT
public:
    Window(WindowOptions options, Theme theme, QWidget *parent = nullptr);
    bool modalVisible() const;
protected:
    void resizeEvent(QResizeEvent *) override;
    void changeEvent(QEvent *) override;
    void closeEvent(QCloseEvent *) override;
private:
    WindowOptions options_;
    CameraService service_;
    ThemeWatcher themeWatcher_;
    QWidget *content_ = nullptr;
    QLabel *brand_ = nullptr;
    QLabel *deviceName_ = nullptr;
    QLabel *previewStatus_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *deviceButton_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *previewButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QLineEdit *search_ = nullptr;
    QScrollArea *controlsScroll_ = nullptr;
    QSplitter *splitter_ = nullptr;
    Preview *preview_ = nullptr;
    Modal *modal_ = nullptr;
    QString selectedDevice_;
    QTimer streamRefresh_;
    void buildInterface();
    QWidget *buildPreviewCard();
    QWidget *buildControlsCard();
    void applyTheme();
    void devicesChanged();
    void selectDevice(const QString &path);
    void rebuildControls();
    void showDevices();
    void showControlMenu(const Control &control);
    void showReset();
    void setStatus(const QString &text, bool error = false);
    void saveSettings();
    void openModal();
};
}
