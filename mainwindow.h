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
    void putTextBackToCursorPosition(const QString &tagColor, QTextCursor &cursor, const QString &selectedText,
                                     const QString &textEnding, QString modifiedText);
    void surroundSelectedTextWithTag(QString text, QString extraAttributes = "",
                                     bool closable = true, QString color = "");
    void surroundSelectedTextWithAHrefTag(QString tagColor = "");


    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
