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

    void onUpdateContextRequested();

    void onStcTagsButtonPressed(StcTags stcTag);

    void onUpdateBreadcrumb();
    void onFileContentChanged(const QString& fileName, int changedLines);

    /// file menu:
    void onNewFilePressed();
    bool onSaveAsPressed();
    bool onSavePressed();
    void onOpenPressed();
    void onExitFromApplicationMenuPressed();
    void onRecentRecentFilesMenuOpened();
    void onCopyFileAbsoluteNamePressed();
    void onCopyFileBaseNamePressed();
    void onOpenParentDirectoryPressed();
    void onShowAvailableShortcutsPressed();
    void onFileStatsRequested();

    /// edit menu:
    void onFindTriggered(bool checked);

    /// help menu:
    void onStcCoursePressed();
    void onCpp0xPl_pressed();
    void onRepository_pressed();

    /// check menu:
    void onCheckTagsPressed();  

protected:
    void setDisabledMenuActionsDependingOnOpenedFile(bool disabled=true);

    QString getCurrentStcContextPath(const QString &text, int cursorPos);

private: // methods
    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);

    void updateContextTable(auto taggedTextLinePositions);
    void connectShortcuts();
    void connectShortcutsFromCodeWidget();
    bool closeApplicationReturningIfClosed();
    bool operationWhichDiscardsChangesRequestedReturningIfDiscarded();
    bool saveEntireContent2File(QString fileName);
    void updateWindowTitle(QString fileName="", QString suffix="");


    void loadSettings();
    void saveSettings();
    void updateRecentFiles(const QString& path);

    QString chooseFileWithDialog(QFileDialog::AcceptMode acceptMode);

private: // members
    Ui::MainWindow *ui;

    QString lastDirectory;
    QMap<QString, int> recentFilesWithPositions;
};
