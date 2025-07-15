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
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    bool loadFileContentToEditorDistargingCurrentContent(QString fileName);

    void onStcTagsButtonPressed(StcTags stcTag);

    void onUpdateBreadcrumb();
    void onFileContentChanged(const QString& fileName, int changedLines);

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
    void onContextShowChanged(bool visible);
    void onGoToLineShowChanged(bool visible);
    void onBreadcrumbVisibilityChanged(bool visible);

    /// help menu:
    void onStcCoursePressed();
    void onCpp0xPl_pressed();
    void onRepository_pressed();

protected:
    void setDisabledMenuActionsDependingOnOpenedFile(bool disabled=true);

    QString getClickableBreadcrumbPath(const QString &text, int cursorPos);

protected slots:
    void onShowStcPreviewTriggered();

private: // methods
    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);

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

private: // members
    Ui::MainWindow *ui;

    QString lastDirectory;

    struct RecentFileInfo
    {
        int cursorPosition;
        QDateTime lastOpened;
    };
    QMap<QString, RecentFileInfo> recentFilesWithPositions;
};
