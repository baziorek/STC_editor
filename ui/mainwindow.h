#pragma once

#include <QMainWindow>
#include <QFileDialog>
#include <QTranslator>
#include "utils/SyntaxMode.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QActionGroup;
class QMenu;
class CppNewsWidget;
QT_END_NAMESPACE

class ConsoleWidget;
class QDockWidget;

class QTextCursor;

enum class StcTags: std::uint32_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct RecentFileInfo
    {
        int cursorPosition;
        int scrollPosition = 0;
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
    void onCopyPreviewHtmlRequested();
    void onEditorViewportMoved();
    void onPreviewClicked(int sourcePosition);

    /// file menu:
    void onNewFilePressed();
    bool onSaveAsPressed();
    bool onSavePressed();
    void onOpenPressed();
    void onReloadFilePressed();
    void onRenameFilePressed();
    void onExitFromApplicationMenuPressed();
    void onRecentRecentFilesMenuOpened();
    void onCopyFileAbsoluteNamePressed();
    void onCopyFileBaseNamePressed();
    void onOpenParentDirectoryPressed();
    void onShowAvailableShortcutsPressed();
    void onFileStatsRequested();

    /// edit menu:
    void onFindTriggered(bool checked);
    void onFindReplaceTriggered(bool checked);
    void onCompareWithPastedTextRequested();

    /// check menu:
    void onCheckTagsPressed();

    /// view menu:
    void onViewMenuAboutToShow();
    void onContextShowChanged(bool visible);
    /// View menu: the news of cpp0x.pl are a tab of the right panel, shown or hidden
    void onCppNewsShowChanged(bool visible);
    void onGoToLineShowChanged(bool visible);
    void onBreadcrumbVisibilityChanged(bool visible);
    void onRightPanelVisibilityChanged(bool visible);
    void onStopWatchVisibilityChanged(bool visible);
    /// View menu: the console (a dock at the bottom), shown with Ctrl+`
    void onConsoleVisibilityChanged(bool visible);

    /// help menu:
    void onStcCoursePressed();
    void onCpp0xPl_pressed();
    void onRepository_pressed();
    void onResumeSessionToggled(bool checked);

    /// language menu:
    void onLanguageChanged();

    /// syntax menu (the mode is also changed by opening a file, so it comes from the editor):
    void onSyntaxModeChanged(SyntaxMode mode);

    /// encoding menu: the entries are made each time a submenu is opened, so they show the state of the open file
    void updateEncodingMenuHeader();
    void rebuildSaveEncodingMenu();
    void rebuildReloadEncodingMenu();
    /// asks whether the file should be converted to the chosen encoding and does it
    void onEncodingChosen(const QString& encodingName);
    /// asks whether the unsaved changes may be lost and reads the file again in the chosen encoding
    void onReloadWithEncodingChosen(const QString& encodingName);

protected:
    void setDisabledMenuActionsDependingOnOpenedFile(bool disabled=true);

    void connectSignals2Slots();

    /// every entry of the "Syntax" menu with the mode it selects
    QList<std::pair<QAction*, SyntaxMode>> syntaxMenuActions() const;

    void setUpEncodingMenu();
    /// the encodings offered in the "Encoding" menu, in the order of the menu
    QStringList offeredEncodings() const;

    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();
    void surroundSelectedTextWithImgTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);
    void toggleTagOnSelectedText(const QString& tag);

    void connectShortcuts();
    void restoreLastSession();
    void connectShortcutsFromCodeWidget();
    bool closeApplicationReturningIfClosed();
    bool operationWhichDiscardsChangesRequestedReturningIfDiscarded(const QString &dialogTitle, const QString &dialogMessage,
                                                                    bool restoreEditorAfterwards = true);
    bool saveEntireContent2File(QString fileName);
    void updateWindowTitle(QString fileName="", QString suffix="");

    void loadSettings();
    void saveSettings();
    void updateRecentFiles(const QString& path);

    QString chooseFileWithDialog(QFileDialog::AcceptMode acceptMode);

    void setTodosCounterValue(int todosTotal);

    /// methods to handle recent files:
    QAction *createRecentFileAction(const QString &filePath, const RecentFileInfo &fileInfo);
    void addEmptyRecentFilesLabel();
    void addClearRecentFilesAction();

private: // members
    Ui::MainWindow *ui;

    QString lastDirectory;

    CppNewsWidget* cppNewsWidget = nullptr;
    int cppNewsTabIndex = -1;

    QDockWidget* consoleDockWidget = nullptr;
    ConsoleWidget* consoleWidget = nullptr;
    bool consoleDirectoryInitialized = false; ///< the directory of the console is set when it is shown for the first time

    QMenu* encodingMenu = nullptr;
    QAction* encodingCurrentAction = nullptr; // "Current encoding: ..." (disabled)
    QMenu* saveEncodingMenu = nullptr;
    QMenu* reloadEncodingMenu = nullptr;
    QActionGroup* encodingActions = nullptr; // exactly one encoding of the "Save in encoding" menu is checked

    /// true for a moment after a click in the preview moved the editor (see onPreviewClicked)
    bool previewClickInProgress = false;

    QMap<QString, RecentFileInfo> recentFilesWithPositions;
    
    QTranslator* m_translator = nullptr;
};
