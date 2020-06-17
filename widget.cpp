#include "widget.h"
#include "ui_widget.h"
#include <QTextCodec>
#include <QFileDialog>
#include <chrono>
#include <thread>
#include <string>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    ,m_player(new MediaPlayer)
{
    ui->setupUi(this);
    this->setFixedSize(400,300);
    connect(ui->openFileBtn,SIGNAL(clicked()), this,SLOT(openFile_Slot()));
}

Widget::~Widget()
{
    delete ui;
}

void Widget::openFile_Slot() {

   QString filename = QFileDialog::getOpenFileName(this,
           QObject::tr("Open Video"),
           "/Users/chengkeke/Downloads/","*.mp4 *.flv")
           ;
    if (filename.isEmpty())
    {
        return;
    }

    int ret = m_player->openFile(filename.toUtf8().data());
    if (ret!=0)
    {
        printf("open file error\n");
        return;
    }

    std::thread thread([this](){
        MediaPacket packet;
        while (1)
        {
           int ret = m_player->readPacket(&packet);
           //std::this_thread::sleep_for(std::chrono::milliseconds(100));
           if (ret != 0)
           {
               printf("read packet error\n");
               break;
           } else{
               printf("read packet success\n");
           }
        }
    });
    thread.detach();
}

