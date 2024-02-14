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

private:
    void putTextBackToCursorPosition(QTextCursor &cursor, QString divClass, QString selectedText,
                                     QString textEnding, QString modifiedText);
    void surroundSelectedTextWithAHrefTag();

    void surroundSelectedTextWithTag(QString divClass, QString text, QString extraAttributes = "", bool closable = true);


    Ui::MainWindow *ui;
    void setUpDocumentStyles();
};
#endif // MAINWINDOW_H
