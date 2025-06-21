/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#pragma once

#include <QPlainTextEdit>
#include <QFileSystemWatcher>

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

    QString openedFileName;

public:
    CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    bool noUnsavedChanges() const;

    auto getFileName() const
    {
        return openedFileName;
    }
    void setFileName(const QString& newFileName)
    {
        openedFileName = newFileName;
    }
    void enableWatchingOfFile(const QString& newFileName);

    void restoreStateWhichDoesNotRequireSaving(bool discardChanges=false);

public slots:
    void fileChanged(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;

    void reloadFromFile(bool discardChanges=false);

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    QFileSystemWatcher fileWatcher;
};
