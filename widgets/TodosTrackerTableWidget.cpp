#include <QTableWidget>
#include <QMap>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QHeaderView>
#include <QTimer>
#include "codeeditor.h"
#include "TodosTrackerTableWidget.h"


TodoTrackerTableWidget::TodoTrackerTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();
}

void TodoTrackerTableWidget::setTextEditor(CodeEditor *newEditor)
{
    if (textEditor)
    {
        disconnect(textEditor->document(), &QTextDocument::contentsChange, this, &TodoTrackerTableWidget::onLineContentChanged);
    }

    textEditor = newEditor;

    if (textEditor)
    {
        connect(textEditor->document(), &QTextDocument::contentsChange, this, &TodoTrackerTableWidget::onLineContentChanged);
        QTimer::singleShot(1000, this, &TodoTrackerTableWidget::scanEntireDocumentDetectingAllTodos);
    }
}

void TodoTrackerTableWidget::clearTodos()
{
    setRowCount(0);
    lineToRowMap.clear();
}

void TodoTrackerTableWidget::onLineContentChanged(int position, int, int)
{
    if (!textEditor)
        return;

    QTextCursor cursor(textEditor->document());
    cursor.setPosition(position);
    int lineNumber = cursor.block().blockNumber();
    QString lineText = cursor.block().text();

    updateOrRemoveTodoForLine(lineNumber, lineText);
}

void TodoTrackerTableWidget::showEvent(QShowEvent *event)
{
    QTableWidget::showEvent(event);
    if (isVisible())
    {
        scanEntireDocumentDetectingAllTodos();
    }
}

void TodoTrackerTableWidget::scanEntireDocumentDetectingAllTodos()
{
    clearTodos();
    if (textEditor)
    {
        const int lineCount = textEditor->linesCount();
        for (int i = 0; i < lineCount; ++i)
        {
            QTextCursor cursor = textEditor->cursor4Line(i + 1);
            updateOrRemoveTodoForLine(i, cursor.block().text());
        }
    }
}

void TodoTrackerTableWidget::setupTable()
{
    setColumnCount(3);
    setHorizontalHeaderLabels({"#", "Line:Pos", "TODO"});
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->hide();
    setAlternatingRowColors(true);
}

void TodoTrackerTableWidget::updateOrRemoveTodoForLine(int lineNumber, const QString &lineText)
{
    QRegularExpressionMatch match = todoRegex.match(lineText);
    if (match.hasMatch())
    {
        QString todoText = match.captured(1).trimmed();

        QTextCursor cursor = textEditor->cursor4Line(lineNumber + 1);
        int column = cursor.positionInBlock();

        if (lineToRowMap.contains(lineNumber))
        {
            int row = lineToRowMap[lineNumber];
            item(row, 1)->setText(QString("%1:%2").arg(lineNumber + 1).arg(column));
            item(row, 2)->setText(todoText);
        }
        else
        {
            int row = rowCount();
            insertRow(row);
            setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
            setItem(row, 1, new QTableWidgetItem(QString("%1:%2").arg(lineNumber + 1).arg(column)));
            setItem(row, 2, new QTableWidgetItem(todoText));
            lineToRowMap[lineNumber] = row;
        }
    }
    else if (lineToRowMap.contains(lineNumber))
    {
        removeTodoRow(lineNumber);
    }
}

void TodoTrackerTableWidget::removeTodoRow(int lineNumber)
{
    int row = lineToRowMap.take(lineNumber);
    removeRow(row);

    // Update lineToRowMap indexes
    QMap<int, int> updated;
    for (auto it = lineToRowMap.begin(); it != lineToRowMap.end(); ++it)
    {
        int oldRow = it.value();
        int updatedRow = oldRow > row ? oldRow - 1 : oldRow;
        updated[it.key()] = updatedRow;
    }
    lineToRowMap = updated;
}
