#pragma once

#include <QMainWindow>
#include <QFileDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTextCursor;

enum class StcTags: std::uint32_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct RecentFileInfo
    {
        int cursorPosition;
        QDateTime lastOpened;
    };


    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    bool loadFileContentToEditorDistargingCurrentContent(const QString& fileName);

    void onStcTagsButtonPressed(StcTags stcTag);

    void onUpdateBreadcrumb();
    void onFileContentChanged(const QString& fileName, int changedLines);

    void onShowStcPreviewTriggered();

    /// file menu:
    void onNewFilePressed();
    bool onSaveAsPressed();
    bool onSavePressed();
    void onOpenPressed();
    void onReloadFilePressed();
    void onExitFromApplicationMenuPressed();
    void onRecentRecentFilesMenuOpened();
    void onCopyFileAbsoluteNamePressed();
    void onCopyFileBaseNamePressed();
    void onOpenParentDirectoryPressed();
    void onShowAvailableShortcutsPressed();
    void onFileStatsRequested();

    /// edit menu:
    void onFindTriggered(bool checked);

    /// check menu:
    void onCheckTagsPressed();

    /// view menu:
    void onViewMenuAboutToShow();
    void onContextShowChanged(bool visible);
    void onGoToLineShowChanged(bool visible);
    void onBreadcrumbVisibilityChanged(bool visible);

    /// help menu:
    void onStcCoursePressed();
    void onCpp0xPl_pressed();
    void onRepository_pressed();

protected:
    void setDisabledMenuActionsDependingOnOpenedFile(bool disabled=true);

    void connectSignals2Slots();

    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();
    void surroundSelectedTextWithImgTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);
    void toggleTagOnSelectedText(const QString& tag);

    void connectShortcuts();
    void connectShortcutsFromCodeWidget();
    bool closeApplicationReturningIfClosed();
    bool operationWhichDiscardsChangesRequestedReturningIfDiscarded(const QString &dialogTitle, const QString &dialogMessage);
    bool saveEntireContent2File(QString fileName);
    void updateWindowTitle(QString fileName="", QString suffix="");

    void loadSettings();
    void saveSettings();
    void updateRecentFiles(const QString& path);

    QString chooseFileWithDialog(QFileDialog::AcceptMode acceptMode);

    /// methods to handle recent files:
    QAction *createRecentFileAction(const QString &filePath, const RecentFileInfo &fileInfo);
    void addEmptyRecentFilesLabel();
    void addClearRecentFilesAction();

private: // members
    Ui::MainWindow *ui;

    QString lastDirectory;

    QMap<QString, RecentFileInfo> recentFilesWithPositions;
};
