#pragma once

#include "core/controls.h"
#include "core/theme.h"
#include <QWidget>

class QLineEdit;
class QPushButton;
class QSlider;
class QTimer;
class QVBoxLayout;

namespace webcam {
class ControlRow : public QWidget {
    Q_OBJECT
public:
    explicit ControlRow(Control control, QWidget *parent = nullptr);
    const Control &control() const { return control_; }
    void applyTheme(const Theme &theme);
signals:
    void writeRequested(const QString &name, const QString &value);
    void menuRequested(const webcam::Control &control);
private:
    Control control_;
    QPushButton *reset_ = nullptr;
    QPushButton *choice_ = nullptr;
    QLineEdit *value_ = nullptr;
    QSlider *slider_ = nullptr;
    void buildEditor(QVBoxLayout *layout);
    void submit(const QString &value);
};
}
