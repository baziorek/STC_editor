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
    a.setOrganizationName("Personal");
    a.setApplicationName("Cpp0x tags editor");
    setUpIcon(a);

    MainWindow w;
    w.show();
    return a.exec();
}

/**
 * TODOs: Cpp0x
 * ! Argumenty uruchomienia
 * 1. Consider using syntax analizer for C++: https://github.com/westes/flex
 * 2. Syntax highlinkk of Cpp & python: QCXXHighlighter https://github.com/Megaxela/QCodeEditor (MIT licence)
 * 3. ! Ctrl + F, Ctrl + R
 * 4. ! Correct formatting: when multi-line code sections
 * 5. TODOs tracking
 * 6. Bookmarks to places in code
 * 7. Add checking if [run] tags are inside [pkt] if there are any tags
 * 8. Tables creator
 * 9. Add checking: if all tags are closed
 * 10. ALT + <- lub ALT + -> umożliwiające skakanie po miejscach w kodzie - wstecz i dalej
 * 11. Listen external changes of file
 * 12. Obrazki na serwerze - podmiana prefixów w linkach
 * 13. Śledzenie zmian - wyświetlanie linii gdzie coś zmieniono
 * 14. *Images preview
 * 15. * Mark lines with changes
 * 16. * Add formatting of C++ code
 * 17. * Add compile C++ code
 * 18. Drag & drop file
 * 19. Lista skrótów
 * 20. Copy file name
 * 21. Umiejscowienie danej pozycji w rozdziałach i divach
 * 22. Statystyki sekcji
 * 23. Procentowo, ile linii na ile widać
 * 24. Pluginy i może LUA
 * 25. Spellcheck polski np. https://github.com/nuspell/nuspell https://doc.qt.io/qt-6/qtwebengine-webenginewidgets-spellchecker-example.html
 * 26. Makra do nagrywania
 * 27. obsługa różnych kodowań plików z rozpoznawaniem
 * 28. podgląd web (kursu STC)
 * 29. Podgląd dokumentacji cppreference (jak cppman da radę i QtCreator)
 **/
