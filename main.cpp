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

    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        QString fileName = args[i];
        QFileInfo fileInfo(fileName);
        if (fileInfo.exists() && fileInfo.isFile())
        {
            if (!w.loadFileContentToEditorDistargingCurrentContent(fileName))
            {
                qWarning() << "Opening file: '" << fileName << "' failed!";
            }
        }
        else
        {
            qWarning() << "File '" << fileName << "' does not exist or is not a file!";
        }
    }

    w.show();
    return a.exec();
}

/**
 * TODOs: Cpp0x
 * 1. ! Ctrl + F, Ctrl + R (with possibility to uncheck)
 * 2. ! Correct formatting: when multi-line code sections
 * 3. ! Coś geometria się nie zapisuje - jedynie położenie, ale nie rozmiar okna
 * 4. Consider using syntax analizer for C++: https://github.com/westes/flex
 * 5. Syntax highlinkk of Cpp & python: QCXXHighlighter https://github.com/Megaxela/QCodeEditor (MIT licence)
 * 6. Bookmarks to places in code
 * 7. Add checking if [run] tags are inside [pkt] if there are any tags
 * 8. Add checking: if all tags are closed
 * 9. Tables creator
 * 10. ALT + <- lub ALT + -> umożliwiające skakanie po miejscach w kodzie - wstecz i dalej
 * 11. TAB - przesuwa zaznaczony tekst w prawo
 *      SHIFT TAB - odwrotnie do powyższego
 * 12. Obrazki na serwerze - podmiana prefixów w linkach
 * 13. Śledzenie zmian - wyświetlanie linii gdzie coś zmieniono
 * 14. TODOs tracking
 * 15. uruchamianie z argumentem trwa wolno - gdy plik jest duży.
 * 16. * Add formatting of C++ code
 * 17. * Add compile C++ code
 * 18. * Mark lines with changes
 * 19. Lista skrótów
 * 20. Spellcheck polski np. https://github.com/nuspell/nuspell https://doc.qt.io/qt-6/qtwebengine-webenginewidgets-spellchecker-example.html
 * 21. Umiejscowienie danej pozycji w rozdziałach i divach
 * 22. Statystyki sekcji
 * 23. podgląd web (kursu STC)
 * 24. Pluginy i może LUA
 * 25. Dodawanie uwag do plików
 * 26. Makra do nagrywania
 * 27. obsługa różnych kodowań plików z rozpoznawaniem
 * 28. Podgląd dokumentacji cppreference (jak cppman da radę i QtCreator)
 **/
