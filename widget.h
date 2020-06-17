#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <thread>
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
    void openFile_Slot();
private:
    Ui::Widget *ui;
    MediaPlayer *m_player = nullptr;
    std::thread  m_playThread;
};
#endif // WIDGET_H
