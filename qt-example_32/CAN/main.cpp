#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/icons/app.png"));
    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/icons/app.png"));
    w.show();

    return a.exec();
}
