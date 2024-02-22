#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QTextCursor;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void keyPressEvent(QKeyEvent *event) override;

    void onUpdateContextRequested();
    void onContextTableClicked(int row, int /*column*/);

    void onSaveAsPressed();
    void onOpenPressed();

private: // methods
    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);

    void setUpDocumentStyles();
    void updateContextTable(auto taggedTextLinePositions);
    void connectButtons();

private: // members
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
