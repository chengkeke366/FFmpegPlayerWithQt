#include <QApplication>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>
#include "widget.h"
int main(int argc, char* argv[])

{
	QApplication a(argc, argv);
	Widget w;
	w.show();
	return a.exec();
	printf("exit\n");
}

