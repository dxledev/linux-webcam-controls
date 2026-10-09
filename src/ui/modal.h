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
class Modal : public QWidget {
    Q_OBJECT
public:
    Modal(QWidget *parent, QWidget *background);
    void applyTheme(const Theme &theme);
    void open(const QString &title, const QString &description, QWidget *body);
    void dismiss();
    void choices(const QString &title, const QString &description, const QVector<QPair<QString, QString>> &options,
                 const QString &selected, std::function<void(QString)> choose);
    void confirm(const QString &title, const QString &description, const QString &action, std::function<void()> confirm);
signals:
    void dismissed();
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    QWidget *background_;
    QFrame *card_;
    QVBoxLayout *bodyLayout_;
    QLabel *title_;
    QLabel *description_;
    QPushButton *close_;
    QWidget *body_ = nullptr;
    QPixmap snapshot_;
    QColor tint_;
    QPointer<QWidget> previousFocus_;
    void capture();
    void centerCard();
};
}
