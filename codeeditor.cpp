/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#include <QFile>
#include <QPainter>
#include <QTextBlock>
#include <qmessagebox.h>
#include "codeeditor.h"
#include "linenumberarea.h"


CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::totalLinesCountChanged);
    // connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    // highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth()
{
    const int maxLine = linesCount();
    const int digits4LineNumber = std::log10(maxLine) + 1;

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits4LineNumber;
    return space;
}

bool CodeEditor::noUnsavedChanges() const
{
    const auto currentlyVisibleText = toPlainText();
    if (openedFileName.isEmpty())
    {
        return currentlyVisibleText.isEmpty();
    }

    QFile file(openedFileName);
    if (!file.exists())
    {
        return false;
    }
    //  TODO: consider: !file.exists() && currentlyVisibleText.isEmpty()

    file.open(QFile::ReadOnly);
    return file.readAll() == currentlyVisibleText;
}

void CodeEditor::enableWatchingOfFile(const QString &newFileName)
{
    auto watchedFilesCopy = fileWatcher.files();
    fileWatcher.removePaths(watchedFilesCopy);
    fileWatcher.addPath(newFileName);
}

void CodeEditor::fileChanged(const QString &path)
{
    // TODO:
#warning tutaj
    // https://doc.qt.io/qt-6/qfilesystemwatcher.html
}

void CodeEditor::go2LineRequested(int lineNumber)
{
    lineNumber = std::clamp(lineNumber, 1, linesCount());

    QTextBlock block = document()->findBlockByLineNumber(lineNumber - 1); // numeration from 0
    if (! block.isValid())
    {
        return;
    }

    QTextCursor cursor(block);
    setTextCursor(cursor);
    ensureCursorVisible();
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::reloadFromFile(bool discardChanges)
{
    QFile file(openedFileName);
    if (!file.exists())
    {
        // return false;
        QMessageBox::warning(this, "Reloading error", "File '" + openedFileName + "' does not exist!");
    }
    //  TODO: consider: !file.exists() && currentlyVisibleText.isEmpty()
    else
    {
        file.open(QFile::ReadOnly);
        setPlainText(file.readAll());
    }
}

void CodeEditor::restoreStateWhichDoesNotRequireSaving(bool discardChanges)
{
    if (openedFileName.isEmpty())
    {
        clear();
    }
    else
    {
        reloadFromFile(discardChanges);
    }
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
