#include <QApplication>
#include <QTimer>
#include <QDebug>
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
