#include "widget.h"
#include "ui_widget.h"
#include <QTextCodec>
#include <QFileDialog>
#include <chrono>
#include <thread>
#include <string>
#include <stdio.h>


Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    ,m_player(new MediaPlayer)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
	delete m_player;
}

void Widget::on_openFileBtn_clicked()
{
	QString filename = QFileDialog::getOpenFileName(this,
		QObject::tr("Open Video"),
		".", "*.mp4 *.flv")
		;
	if (filename.isEmpty())
	{
		return;
	}
	m_player->registerRenderWindowsCallback(ui->widget);
	m_player->start_play(filename.toUtf8().data());
}

void Widget::on_play_paush_btn_clicked()
{
	m_player->pause_resume_play();
}

