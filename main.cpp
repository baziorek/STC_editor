#include "ui/mainwindow.h"

#include <QApplication>


void setUpIcon(QApplication& a)
{
    QIcon appIcon(":/resources/icon.png");
    if (appIcon.isNull())
    {
        qWarning() << "Nie udało się załadować ikony aplikacji!";
    }
    else
    {
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
 * 1. Consider using syntax analizer for C++: https://github.com/westes/flex
 * 2. ! Correct formatting: when multi-line code sections
 * 3. ! Ctrl + O does not ask if changes!
 * 4. ALT + <- lub ALT + -> umożliwiające skakanie po miejscach w kodzie - wstecz i dalej
 * 5. Ctrl + F, Ctrl + R
 * 6. Add checking: if all tags are closed
 * 7. *Images preview
 * 8. Tables creator
 * 9. Better close event https://stackoverflow.com/questions/10417914/handling-exit-without-saving-in-qt https://forum.qt.io/topic/31194/the-right-way-to-do-something-before-closing-the-program-closeevent-abouttoquit-or-the-destructor/2
 * 10. Syntax highlinkk of Cpp & python: QCXXHighlighter https://github.com/Megaxela/QCodeEditor (MIT licence)
 * 11. Listen external changes of file
 * 12. Obrazki na serwerze - podmiana prefixów w linkach
 * 13. Śledzenie TODOsów
 * 14. Zakładki na miejsca w kodzie

 **/
