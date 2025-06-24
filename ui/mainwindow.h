#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTextCursor;

enum class StdTags: std::uint8_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    void onUpdateContextRequested();
    void onContextTableClicked(int row, int /*column*/);

    bool onSaveAsPressed();
    bool onSavePressed();
    void onOpenPressed();

    void onCheckTagsPressed();

    void onStcTagsButtonPressed(StdTags stcTag);

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
    void updateWindowTitle(QString fileName);

private: // members
    Ui::MainWindow *ui;
};
