#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}

/**
 *
 * TODO:
 * 1. Consider using syntax analizer: https://github.com/westes/flex https://stackoverflow.com/questions/2656809/how-do-you-implement-syntax-highlighting
 * 2. Maybe tags gray, normal text black?
 * 3. Cytat
 **/
