/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#include "LineNumberArea.h"
#include "../CodeEditor.h"


LineNumberArea::LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor)
{}

QSize LineNumberArea::sizeHint() const
{
    return QSize(codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    codeEditor->lineNumberAreaPaintEvent(event);
}
