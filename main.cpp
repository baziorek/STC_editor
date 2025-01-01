#include "ui/mainwindow.h"

#include <QApplication>


void setUpIcon(QApplication& a)
{
    QIcon appIcon(":/resources/icon.png");

    if (appIcon.isNull()) {
        qWarning() << "Nie udało się załadować ikony aplikacji!";
    } else {
        qDebug() << "Ikona aplikacji została poprawnie załadowana.";
        a.setWindowIcon(appIcon);
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    setUpIcon(a);

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
 * 9. Better close event https://stackoverflow.com/questions/10417914/handling-exit-without-saving-in-qt https://forum.qt.io/topic/31194/the-right-way-to-do-something-before-closing-the-program-closeevent-abouttoquit-or-the-destructor/2
 * 10. Syntax highlinkk of Cpp & python: QCXXHighlighter https://github.com/Megaxela/QCodeEditor (MIT licence)
 * 11. Listen external changes of file
 * 12. Obrazki na serwerze - podmiana prefixów w linkach
 * 13. Jak jest jakiś tekst napisany od zera (bez wskazania pliku) to chyba dobrze byłoby zapytać użytkownika czy chce wyjść niezapisując?
 **/
