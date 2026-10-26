#pragma once

#include "core/theme.h"
#include <QPointer>
#include <QPixmap>
#include <QWidget>
#include <functional>

class QFrame;
class QVBoxLayout;
class QLabel;
class QPushButton;

namespace webcam {
class ModalMotion;
class Modal : public QWidget {
    Q_OBJECT
public:
    Modal(QWidget *parent, QWidget *background);
    void applyTheme(const Theme &theme);
    void open(const QString &title, const QString &description, QWidget *body);
    void dismiss();
    bool closing() const { return closing_; }
    void choices(const QString &title, const QString &description, const QVector<QPair<QString, QString>> &options,
                 const QString &selected, std::function<void(QString)> choose);
    void confirm(const QString &title, const QString &description, const QString &action, std::function<void()> confirm);
signals:
    void dismissed();
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void hideEvent(QHideEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QWidget *background_;
    QFrame *card_;
    QVBoxLayout *bodyLayout_;
    QLabel *title_;
    QLabel *description_;
    QPushButton *close_;
    QWidget *body_ = nullptr;
    QPixmap snapshot_, backgroundFrame_;
    QColor tint_;
    ModalMotion *motion_;
    std::function<void()> pendingAction_;
    bool closing_ = false, backgroundWasEnabled_ = true;
    bool backgroundFrozen_ = false, backgroundUpdatesWereEnabled_ = true;
    QPointer<QWidget> previousFocus_;
    void capture();
    void beginDismiss(std::function<void()> action);
    void finishDismiss();
    void restoreBackgroundUpdates();
    void centerCard();
};
}
