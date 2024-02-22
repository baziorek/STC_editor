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
 * TODOs:
 * 1. Consider using syntax analizer: https://github.com/westes/flex https://stackoverflow.com/questions/2656809/how-do-you-implement-syntax-highlighting
 * 2. Maybe tags gray, normal text black, or tags smaller?
 * 3. Add syntax highlighter:
 * https://doc.qt.io/qt-6.2/qtwidgets-richtext-syntaxhighlighter-example.html
 * 4. Warn before closing when not saved changes
 * 5. Ctrl + F, Ctrl + R
 * 6. Add checking: if all tags are closed
 * 7. *Images preview
 * 8. Tables creator
 **/
