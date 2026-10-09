#include "control_row.h"
#include "icons.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace webcam {
ControlRow::ControlRow(Control control, QWidget *parent) : QWidget(parent), control_(std::move(control)) {
    setObjectName("controlRow");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 12);
    layout->setSpacing(8);
    auto *header = new QHBoxLayout;
    auto *name = new QLabel(control_.label());
    name->setTextFormat(Qt::PlainText);
    name->setWordWrap(true);
    header->addWidget(name, 1);
    if (control_.canReset()) {
        reset_ = new QPushButton;
        reset_->setObjectName("resetButton");
        reset_->setFixedSize(28, 28);
        reset_->setAccessibleName("Reset " + control_.label() + " to " + control_.defaultValue);
        connect(reset_, &QPushButton::clicked, this, [this] { submit(control_.defaultValue); });
        header->addWidget(reset_);
    }
    layout->addLayout(header);
    buildEditor(layout);
    if (!control_.limitation().isEmpty()) {
        auto *limitation = new QLabel(control_.limitation());
        limitation->setObjectName("smallMuted");
        limitation->setWordWrap(true);
        layout->addWidget(limitation);
    }
}

void ControlRow::submit(const QString &value) { emit writeRequested(control_.name, value); }

void ControlRow::buildEditor(QVBoxLayout *layout) {
    switch (control_.kind()) {
    case ControlKind::Toggle: {
        auto *toggle = new QCheckBox(control_.value == "1" ? "Enabled" : "Disabled");
        toggle->setAccessibleName(control_.label());
        toggle->setChecked(control_.value == "1");
        toggle->setEnabled(!control_.blocked());
        connect(toggle, &QCheckBox::clicked, this, [this](bool checked) { submit(checked ? "1" : "0"); });
        layout->addWidget(toggle);
        break;
    }
    case ControlKind::Menu:
        choice_ = new QPushButton(control_.valueLabel());
        choice_->setObjectName("choiceButton");
        choice_->setEnabled(!control_.blocked());
        choice_->setAccessibleName(control_.label());
        connect(choice_, &QPushButton::clicked, this, [this] { emit menuRequested(control_); });
        layout->addWidget(choice_);
        break;
    case ControlKind::Slider: {
        auto *row = new QHBoxLayout;
        slider_ = new QSlider(Qt::Horizontal);
        slider_->setRange(static_cast<int>(control_.minimum), static_cast<int>(control_.maximum));
        slider_->setSingleStep(static_cast<int>(std::min<qint64>(control_.step, INT_MAX)));
        slider_->setPageStep(std::max(slider_->singleStep(), (slider_->maximum() - slider_->minimum()) / 10));
        slider_->setValue(control_.value.toInt());
        slider_->setAccessibleName(control_.label());
        slider_->setEnabled(!control_.blocked());
        auto *value = new QLabel(control_.value);
        value->setObjectName("controlValue");
        value->setMinimumWidth(44);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(slider_, 1);
        row->addWidget(value);
        auto *debounce = new QTimer(this);
        debounce->setSingleShot(true);
        debounce->setInterval(180);
        connect(slider_, &QSlider::valueChanged, this, [this, value, debounce](int number) {
            const qint64 offset = static_cast<qint64>(number) - control_.minimum;
            const auto snapped = control_.minimum + (offset / control_.step) * control_.step;
            value->setText(QString::number(snapped));
            if (!slider_->isSliderDown())
                debounce->start();
        });
        auto commit = [this, value, debounce] { debounce->stop(); submit(value->text()); };
        connect(slider_, &QSlider::sliderReleased, this, commit);
        connect(debounce, &QTimer::timeout, this, commit);
        layout->addLayout(row);
        break;
    }
    case ControlKind::Action: {
        auto *action = new QPushButton("Run action");
        action->setEnabled(!control_.blocked());
        connect(action, &QPushButton::clicked, this, [this] { submit("1"); });
        layout->addWidget(action);
        break;
    }
    case ControlKind::Value: {
        auto *row = new QHBoxLayout;
        value_ = new QLineEdit(control_.value);
        value_->setContextMenuPolicy(Qt::NoContextMenu);
        value_->setMaxLength(4096);
        value_->setAccessibleName(control_.label());
        auto *apply = new QPushButton("Apply");
        value_->setEnabled(!control_.blocked());
        apply->setEnabled(!control_.blocked());
        connect(apply, &QPushButton::clicked, this, [this] { submit(value_->text()); });
        connect(value_, &QLineEdit::returnPressed, apply, &QPushButton::click);
        row->addWidget(value_, 1);
        row->addWidget(apply);
        layout->addLayout(row);
        break;
    }
    case ControlKind::ReadOnly: {
        auto *value = new QLabel(control_.valueLabel());
        value->setObjectName("muted");
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        layout->addWidget(value);
        break;
    }
    }
}

void ControlRow::applyTheme(const Theme &theme) {
    if (reset_)
        reset_->setIcon(themedIcon(Icon::Reset, theme.color("muted"), devicePixelRatioF()));
    if (choice_)
        choice_->setIcon(themedIcon(Icon::Chevron, theme.color("muted"), devicePixelRatioF()));
}
}
