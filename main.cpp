#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QSettings>
#include "ui/mainwindow.h"


void setUpIcon(QApplication& a);
void loadTranslations(QApplication& a, const QString& language = "en");

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Personal");
    a.setApplicationName("Cpp0x tags editor");
    
    // Load translations
    QSettings settings;
    QString savedLanguage = settings.value("language", "en").toString();
    loadTranslations(a, savedLanguage);
    
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

void loadTranslations(QApplication& a, const QString& language)
{
    static QTranslator translator;
    static QTranslator qtTranslator;
    
    // Remove any previously loaded translators
    a.removeTranslator(&translator);
    a.removeTranslator(&qtTranslator);
    
    // Load application-specific translations
    QString translationFile = ":/translations/stc_editor_" + language + ".qm";
    if (translator.load(translationFile)) {
        a.installTranslator(&translator);
    } else {
        qWarning() << "Failed to load translation file for language:" << language;
        qWarning() << "Tried to load:" << translationFile;
    }
    
    // Load Qt standard translations
    if (qtTranslator.load("qt_" + language, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        a.installTranslator(&qtTranslator);
    }
}
