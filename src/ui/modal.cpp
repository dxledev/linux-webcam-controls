#include "modal.h"
#include "icons.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <algorithm>

namespace webcam {
namespace {
QPixmap extendEdges(const QPixmap &snapshot, int padding) {
    const auto source = snapshot.toImage().convertToFormat(QImage::Format_RGB32);
    QImage extended(source.size() + QSize(padding * 2, padding * 2), QImage::Format_RGB32);
    for (int y = 0; y < extended.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(source.constScanLine(std::clamp(y - padding, 0, source.height() - 1)));
        auto *destination = reinterpret_cast<QRgb *>(extended.scanLine(y));
        std::fill_n(destination, padding, row[0]);
        std::copy_n(row, source.width(), destination + padding);
        std::fill_n(destination + padding + source.width(), padding, row[source.width() - 1]);
    }
    return QPixmap::fromImage(extended);
}
}

Modal::Modal(QWidget *parent, QWidget *background) : QWidget(parent), background_(background), card_(new QFrame(this)) {
    setObjectName("modalOverlay");
    setFocusPolicy(Qt::StrongFocus);
    card_->setObjectName("modalCard");
    auto *layout = new QVBoxLayout(card_);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    title_ = new QLabel;
    title_->setObjectName("modalTitle");
    title_->setTextFormat(Qt::PlainText);
    header->addWidget(title_, 1);
    close_ = new QPushButton;
    close_->setObjectName("iconButton");
    close_->setFixedSize(34, 34);
    close_->setAccessibleName("Close dialog");
    connect(close_, &QPushButton::clicked, this, &Modal::dismiss);
    header->addWidget(close_);
    layout->addLayout(header);
    description_ = new QLabel;
    description_->setObjectName("muted");
    description_->setTextFormat(Qt::PlainText);
    description_->setWordWrap(true);
    layout->addWidget(description_);
    bodyLayout_ = new QVBoxLayout;
    bodyLayout_->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(bodyLayout_, 1);
    hide();
}

void Modal::applyTheme(const Theme &theme) {
    tint_ = theme.color("background");
    tint_.setAlpha(105);
    close_->setIcon(themedIcon(Icon::Close, theme.color("text"), devicePixelRatioF()));
    if (isVisible())
        capture();
    update();
}

void Modal::capture() {
    if (background_->size().isEmpty())
        return;
    background_->layout()->activate();
    auto snapshot = background_->grab();
    auto reducedSize = (snapshot.deviceIndependentSize().toSize() / 3).expandedTo(QSize(1, 1));
    if (reducedSize.width() > 640 || reducedSize.height() > 640)
        reducedSize.scale(640, 640, Qt::KeepAspectRatio);
    auto reduced = snapshot.scaled(reducedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    reduced.setDevicePixelRatio(1);
    // Match the reduced, edge-extended blur without retaining a full-window image.
    constexpr int padding = 32;
    QGraphicsScene scene;
    auto *image = scene.addPixmap(extendEdges(reduced, padding));
    auto *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(5);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    image->setGraphicsEffect(blur);
    snapshot_ = QPixmap(reduced.size());
    snapshot_.fill(Qt::transparent);
    QPainter painter(&snapshot_);
    scene.render(&painter, QRectF(QPointF(), reduced.size()), QRectF(QPointF(padding, padding), reduced.size()));
}

void Modal::centerCard() {
    const int width = std::max(1, std::min(540, this->width() - 48));
    card_->setFixedWidth(width);
    card_->setFixedHeight(std::min(std::max(220, card_->sizeHint().height()), std::max(1, height() - 48)));
    card_->move((this->width() - card_->width()) / 2, (height() - card_->height()) / 2);
    card_->layout()->activate();
}

void Modal::open(const QString &title, const QString &description, QWidget *body) {
    if (body_) {
        bodyLayout_->removeWidget(body_);
        body_->deleteLater();
        body_->hide();
    }
    body_ = body;
    bodyLayout_->addWidget(body_);
    title_->setText(title);
    description_->setText(description);
    description_->setVisible(!description.isEmpty());
    previousFocus_ = QApplication::focusWidget();
    setGeometry(parentWidget()->rect());
    capture();
    background_->setEnabled(false);
    show();
    raise();
    centerCard();
    close_->setFocus(Qt::OtherFocusReason);
}

void Modal::dismiss() {
    if (!isVisible())
        return;
    hide();
    snapshot_ = {};
    background_->setEnabled(true);
    if (previousFocus_)
        previousFocus_->setFocus(Qt::OtherFocusReason);
    emit dismissed();
}

void Modal::choices(const QString &title, const QString &description, const QVector<QPair<QString, QString>> &options,
                    const QString &selected, std::function<void(QString)> choose) {
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumHeight(std::min(300, std::max(60, static_cast<int>(options.size()) * 54)));
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    for (const auto &option : options) {
        auto *button = new QPushButton(option.second);
        button->setObjectName(option.first == selected ? "selectedChoice" : "choiceButton");
        button->setMinimumHeight(44);
        connect(button, &QPushButton::clicked, this, [this, choose, value = option.first] { dismiss(); choose(value); });
        layout->addWidget(button);
    }
    layout->addStretch();
    scroll->setWidget(body);
    open(title, description, scroll);
}

void Modal::confirm(const QString &title, const QString &description, const QString &action, std::function<void()> confirm) {
    auto *body = new QWidget;
    auto *layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 12, 0, 0);
    auto *cancel = new QPushButton("Cancel");
    auto *accept = new QPushButton(action);
    accept->setObjectName("primaryButton");
    connect(cancel, &QPushButton::clicked, this, &Modal::dismiss);
    connect(accept, &QPushButton::clicked, this, [this, confirm] { dismiss(); confirm(); });
    layout->addStretch();
    layout->addWidget(cancel);
    layout->addWidget(accept);
    open(title, description, body);
}

void Modal::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(rect(), snapshot_);
    painter.fillRect(rect(), tint_);
}

void Modal::resizeEvent(QResizeEvent *) {
    centerCard();
    if (isVisible())
        capture();
}

void Modal::mousePressEvent(QMouseEvent *event) {
    if (!card_->geometry().contains(event->position().toPoint()))
        dismiss();
    event->accept();
}

void Modal::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        auto *button = qobject_cast<QPushButton *>(QApplication::focusWidget());
        if (button && isAncestorOf(button))
            button->click();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}
}
