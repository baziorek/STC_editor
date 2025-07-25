/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#pragma once

#include <QWidget>

class CodeEditor;

class LineNumberArea : public QWidget
{
public:
    LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    CodeEditor *codeEditor;
};
