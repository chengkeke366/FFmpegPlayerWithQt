#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "MediaPlayer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
	void on_openFileBtn_clicked();
	void on_play_paush_btn_clicked();
private:
    Ui::Widget *ui;
    MediaPlayer *m_player = nullptr;
};
#endif // WIDGET_H
