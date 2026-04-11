#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    QTimer timer ;


    QObject::connect(&timer, &QTimer::timeout, &w, &MainWindow::dowork);

    timer.start(100);



    w.setWindowTitle("电力金具智能检测系统");
    w.show();
    return a.exec();
}
