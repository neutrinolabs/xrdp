
#include <QtGui/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication::setGraphicsSystem(QLatin1String("native"));
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
