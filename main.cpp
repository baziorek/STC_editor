#include <QApplication>
#include <QTimer>
#include "ui/mainwindow.h"


void setUpIcon(QApplication& a);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Personal");
    a.setApplicationName("Cpp0x tags editor");
    setUpIcon(a);

    MainWindow w;

    QStringList filesToOpen;
    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i)
    {
        QString fileName = args[i];
        QFileInfo fileInfo(fileName);
        if (fileInfo.exists() && fileInfo.isFile())
            filesToOpen << fileName;
        else
            qWarning() << "File '" << fileName << "' does not exist or is not a file!";
    }

    if (!filesToOpen.isEmpty())
    {
        constexpr int delayBeforeOpeningFilesInMiliseconds = 300;
        QTimer::singleShot(delayBeforeOpeningFilesInMiliseconds,
                           [&w, filesToOpen]() {
            for (const QString& fileName : filesToOpen)
            {
                if (!w.loadFileContentToEditorDistargingCurrentContent(fileName))
                {
                    qWarning() << "Opening file: '" << fileName << "' failed!";
                }
            }
        });
    }

    w.show();
    return a.exec();
}

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

/**
 * TODOs: Cpp0x
 * TODO Before release:
 * ! Powinna być informacje o tym, że są zmiany w otwartym pliku
 * !! Czasami pyta czy zastąpić zmiany, mimo iż nie ma zmian (jak otwieram sekwencyjnie wiele plików)
 * 1. ! Ctrl + R (with possibility to uncheck)
 * 2. ! Correct formatting: when multi-line code sections
 * 3. ! Coś geometria się nie zapisuje - jedynie położenie, ale nie rozmiar okna
 * 4. Add checking if [run] tags are inside [pkt] if there are any tags
 * 5. Add checking: if all tags are closed
 * 6. Bookmarks to places in code
 * 7. TODOs tracking
 * 8. * Umiejscowienie danej pozycji w rozdziałach i divach
 * 9. Opening multiple files at once
 * 10. Widok sąsiadujący
 * 11. ALT + <- lub ALT + -> umożliwiające skakanie po miejscach w kodzie - wstecz i dalej
 *
 * 1. Compile code
 * 2. Export codes
 * 3. Images to single directory
 * 4. Consider using syntax analizer for C++: https://github.com/westes/flex
 * 5. Syntax highlinkk of Cpp & python: QCXXHighlighter https://github.com/Megaxela/QCodeEditor (MIT licence)
 * 6. Tables creator
 * 7. najlepiej aby od razu wskazywać ile linii zmieniono, ile dodano, ile usunięto
 * 8. Obrazki na serwerze - podmiana prefixów w linkach
 * 9. Śledzenie zmian - wyświetlanie linii gdzie coś zmieniono
 * 10. Multi-search - multiple words in the same lines
 * 11. Spellcheck polski np. https://github.com/nuspell/nuspell https://doc.qt.io/qt-6/qtwebengine-webenginewidgets-spellchecker-example.html
 * 12. obsługa różnych kodowań plików z rozpoznawaniem
 * 13. podgląd web (kursu STC)
 * 14. Pluginy i może LUA
 * 15. Podgląd dokumentacji cppreference (jak cppman da radę i QtCreator)
 * 16. Makra do nagrywania
 * 17. * Add compile C++ code
 * 18. * Mark lines with changes
 *
 * Inspiration: https://github.com/AleksandrHovhannisyan/Scribe-Text-Editor
 **/
