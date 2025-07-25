#include <QTableWidget>
#include <QMap>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QHeaderView>
#include <QTimer>
#include "CodeEditor.h"
#include "TodosTrackerTableWidget.h"


namespace
{
QList<const TodoTrackerTableWidget::TodoInfo*> sortedTagsCopy(const QList<TodoTrackerTableWidget::TodoInfo>& todoList)
{
    QList<const TodoTrackerTableWidget::TodoInfo*> sortedTodos;
    for (const auto& todo : todoList)
        sortedTodos << &todo;

    std::sort(sortedTodos.begin(), sortedTodos.end(), [](const TodoTrackerTableWidget::TodoInfo* a, const TodoTrackerTableWidget::TodoInfo* b) {
        int lineA = a->cursor.block().blockNumber();
        int lineB = b->cursor.block().blockNumber();
        if (lineA != lineB)
            return lineA < lineB;
        return a->cursor.positionInBlock() < b->cursor.positionInBlock();
    });
    return sortedTodos;
}
} // namespace

TodoTrackerTableWidget::TodoTrackerTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();

    connect(this, &QTableWidget::cellClicked, this, &TodoTrackerTableWidget::onCellSingleClicked);
}

void TodoTrackerTableWidget::setTextEditor(CodeEditor *newEditor)
{
    if (textEditor)
    {
        disconnect(textEditor->document(), &QTextDocument::contentsChange, this, &TodoTrackerTableWidget::onLineContentChanged);

        disconnect(this, &TodoTrackerTableWidget::goToLineAndOffsetRequested, textEditor, &CodeEditor::goToLineAndOffset);
        disconnect(this, &TodoTrackerTableWidget::goToLineRequested, textEditor, &CodeEditor::go2LineRequested);
    }

    textEditor = newEditor;

    if (textEditor)
    {
        connect(textEditor->document(), &QTextDocument::contentsChange, this, &TodoTrackerTableWidget::onLineContentChanged);

        connect(this, &TodoTrackerTableWidget::goToLineAndOffsetRequested, textEditor, &CodeEditor::goToLineAndOffset);
        connect(this, &TodoTrackerTableWidget::goToLineRequested, textEditor, &CodeEditor::go2LineRequested);

        QTimer::singleShot(500, this, &TodoTrackerTableWidget::scanEntireDocumentDetectingAllTodos);
    }
}

void TodoTrackerTableWidget::onLineContentChanged(int position, int charsRemoved, int charsAdded)
{
    if (!textEditor)
        return;

    QTextCursor cursor(textEditor->document());
    cursor.setPosition(position);

    QTextBlock block = cursor.block();
    const QString lineText = block.text();

    // update existing TODO or remove it
    QRegularExpressionMatch match = todoRegex.match(lineText);
    auto it = std::find_if(todoList.begin(), todoList.end(),
                           [&block](const TodoInfo& info) {
                               return info.cursor.block() == block;
                           });

    if (match.hasMatch())
    {
        QString todoText = match.captured(1).trimmed();
        if (it != todoList.end())
        {
            it->text = todoText;
            it->cursor.setPosition(block.position() + match.capturedStart());
        }
        else
        {
            TodoInfo newTodo;
            newTodo.cursor = QTextCursor(block);
            newTodo.text = todoText;
            todoList.append(newTodo);
        }
    }
    else if (it != todoList.end())
    {
        todoList.erase(it);
    }

    refreshTable();
}

void TodoTrackerTableWidget::refreshTable()
{
    clearContents();

    QList<const TodoInfo*> sortedTodos = sortedTagsCopy(todoList);

    int index = 0;
    for (const TodoInfo* todo : sortedTodos)
    {
        int line = todo->cursor.block().blockNumber() + 1;
        int posInLine = todo->cursor.positionInBlock();
        insertRow(index);
        setItem(index, 0, new QTableWidgetItem(QString::number(index + 1)));
        setItem(index, 1, new QTableWidgetItem(QString("%1:%2").arg(line).arg(posInLine)));
        setItem(index, 2, new QTableWidgetItem(todo->text));
        ++index;
    }

    emit todosTotalCountChanged(todoList.size());
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
    todoList.clear();

    QTextDocument* doc = textEditor->document();
    for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next())
    {
        QString lineText = block.text();
        QRegularExpressionMatch match = todoRegex.match(lineText);
        if (match.hasMatch())
        {
            TodoInfo info;
            info.cursor = QTextCursor(block);
            info.cursor.setPosition(block.position() + match.capturedStart());
            info.text = match.captured(1).trimmed();
            todoList.append(info);
        }
    }

    refreshTable();    
}

void TodoTrackerTableWidget::onCellSingleClicked(int row, int)
{
    QTableWidgetItem* item = this->item(row, 1);
    if (!item)
        return;

    auto positionLineColonColumn = item->text();
    if (positionLineColonColumn.isEmpty())
    {
        return;
    }

    auto positionLineColonColumnSplitted = positionLineColonColumn.split(':');

    bool ok;
    const auto lineNumber = positionLineColonColumnSplitted.front().toInt(&ok);

    if (! ok) // parsing number failed
    {
        return;
    }

    if (positionLineColonColumnSplitted.size() > 1)
    {
        auto positionInLine = positionLineColonColumnSplitted[1].toInt(&ok);
        if (ok)
        {
            emit goToLineAndOffsetRequested(lineNumber, positionInLine);
            return;
        }
    }

    emit goToLineRequested(lineNumber);
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

void TodoTrackerTableWidget::clearTodos()
{
    todoList.clear();
    setRowCount(0);
    clearContents();
    emit todosTotalCountChanged(0);
}
