#pragma once

#include <QMainWindow>
#include <QFileDialog>
#include <QFont>
#include <QTranslator>
#include "utils/SyntaxMode.h"

#include <QMap>
#include <QPair>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QActionGroup;
class QMenu;
class CppNewsWidget;
QT_END_NAMESPACE

class ConsoleWidget;
class CodeEditor;
class QDockWidget;
class QLabel;
class QTimer;
class MinimapWidget;

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
    void openFileInTab(const QString& fileName);

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
    void onPrintPressed();
    void onExportPdfPressed();
    void onInsertDatetimePressed();
    void onReopenClosedTabPressed();
    void onNavigateBackPressed();
    void onNavigateForwardPressed();
    void onCompareWithFileRequested();
    void onCompareFilesRequested();
    void onEditorTabsTabBarContextMenu(const QPoint& pos);
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
    void onWordWrapChanged(bool enabled);
    void onAutoIndentChanged(bool enabled);
    void onEditorFontClicked();
    void onMinimapChanged(bool enabled);
    /// zen mode (F11): hides every panel, bar and dock and goes fullscreen; the previous
    /// visibility of each widget is remembered and put back on the way out
    void onZenModeChanged(bool zen);
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

    /// middle click on a document tab closes it
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    /// document tabs: each tab holds its own CodeEditor with the whole state of the document
    CodeEditor* currentEditor() const;
    CodeEditor* createEditorTab();
    void connectEditorSignals(CodeEditor* editor);
    void onEditorTabsCurrentChanged(int index);
    void onEditorTabCloseRequested(int index);
    /// asks about unsaved changes of the current tab and closes it; returns false when the user cancelled.
    /// With ensureAtLeastOneTab=false (application close) the last tab is not replaced by a fresh one.
    bool confirmAndCloseCurrentTab(bool ensureAtLeastOneTab = true);
    void updateTabTitle(CodeEditor* editor);
    void refreshEditorDependentWidgets(CodeEditor* editor);
    /// the counts and the cursor position of the status bar, of the document of the current tab
    void updateStatusBarCounts(CodeEditor* editor);
    void updateStatusBarPosition(CodeEditor* editor);
    /// restarts the short delay after which the counts are counted again (they are expensive on long documents)
    void scheduleStatusBarCountsUpdate();

    /// closed document tabs, for "Reopen closed tab" (Ctrl+Shift+T); the newest is at the back
    struct ClosedTabInfo
    {
        QString fileName;        ///< empty for a document which never got a name
        QString untitledText;    ///< the content of a nameless document (empty for named ones: they are read again)
        int cursorPosition = 0;
        int scrollValue = 0;
    };
    void rememberClosedTab(CodeEditor* editor);
    void restoreClosedTabPosition(CodeEditor* editor, int cursorPosition, int scrollValue);
    /// the context menu of the tab bar: close the tab under the click, the other tabs or all of them
    void closeOtherTabsThan(int keepIndex);
    void closeAllTabs();
    /// opens the file in a tab: activates the tab if it is already open, reuses a pristine empty tab or adds a new one
    int findTabIndexForFile(const QString& fileName) const;
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

    /// word wrap of the document editors, kept in the settings (View menu → Word wrap)
    bool wordWrapEnabled = true;

    /// auto-indent inside code blocks (View menu → Auto-indent), kept in the settings
    bool autoIndentEnabled = true;

    /// the font of every document editor, kept in the settings (View menu → Editor font…)
    QFont editorFont;

    /// the right side of the status bar: the counts of the document and the position of the cursor
    QLabel* statusCountsLabel = nullptr;
    QLabel* statusPositionLabel = nullptr;
    /// delays the recount of the words and the characters after the last change of the text
    QTimer* statusCountsTimer = nullptr;

    /// the last closed document tabs, the most recently closed at the back (Ctrl+Shift+T)
    QVector<ClosedTabInfo> closedTabs;

    /// the miniature of the document on the right of the editor tabs (View menu → Minimap)
    MinimapWidget* minimapWidget = nullptr;

    /// zen mode (F11): the window is fullscreen and every panel, bar and dock is hidden
    bool zenModeActive = false;
    /// what was visible when zen mode was entered, restored when it is left
    QVector<QPair<QWidget*, bool>> zenWidgetStates;

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
