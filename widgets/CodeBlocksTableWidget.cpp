#include <QHeaderView>
#include <QAction>
#include <QTextBlock>
#include <QToolTip>
#include "widgets/CodeBlocksTableWidget.h"
#include "CodeEditor.h"
#include "types/CodeBlock.h"

namespace
{
    enum Columns
    {
        Position = 0,
        Type = 1,
        Code = 2,
        ColumnCount
    };

    void setCellValue(QTableWidget *table, int row, int column, const QString &value, const QString &toolTip = QString())
    {
        auto *tableItem = table->item(row, column);
        if (! tableItem)
        {
            tableItem = new QTableWidgetItem;
            table->setItem(row, column, tableItem);
        }

        tableItem->setData(Qt::DisplayRole, value);
        tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
        if (! toolTip.isEmpty())
        {
            tableItem->setToolTip(toolTip);
        }
    }
} // namespace


CodeBlocksTableWidget::CodeBlocksTableWidget(QWidget *parent)
    : QTableWidget(parent)
{
    setupTable();

    createFilterMenu();

    connect(this, &QTableWidget::cellClicked, this, &CodeBlocksTableWidget::onCellClicked);
}

void CodeBlocksTableWidget::setupTable()
{
    setColumnCount(ColumnCount);
    setHorizontalHeaderLabels({"Position", "Type", "Code"});
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    horizontalHeader()->setSectionResizeMode(Position, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(Type, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(Code, QHeaderView::Stretch);
    verticalHeader()->hide();

    // Enable custom tooltips
    setMouseTracking(true);
}

void CodeBlocksTableWidget::setTextEditor(CodeEditor *newTextEditor)
{
    if (textEditor)
    {
        disconnect(textEditor, &CodeEditor::codeBlocksChanged, this, &CodeBlocksTableWidget::updateCodeBlocks);
    }

    textEditor = newTextEditor;

    if (textEditor)
    {
        connect(textEditor, &CodeEditor::codeBlocksChanged, this, &CodeBlocksTableWidget::updateCodeBlocks);
        updateCodeBlocks();
    }
}

QString CodeBlocksTableWidget::getDisplayNameFromCodeType(const QString &tag, const QString &language) const
{
    if (tag == "code" && !language.isEmpty())
    {
        return QString("code %1").arg(language);
    }
    return tag;
}

QString CodeBlocksTableWidget::getFilterCategory4CodeBlock(const QString &tag, const QString &language) const
{
    if (tag == "cpp" || (tag == "code" && language.contains("C++", Qt::CaseInsensitive)))
    {
        return FILTER_CPP;
    }
    if (tag == "py" || (tag == "code" && language.contains("Python", Qt::CaseInsensitive)))
    {
        return FILTER_PYTHON;
    }
    return FILTER_GENERAL;
}

void CodeBlocksTableWidget::updateCodeBlocks()
{
    clearContents();
    setRowCount(0);

    if (!textEditor)
        return;

    const auto &blocks = textEditor->getCodeBlocks();
    for (const auto &block: blocks)
    {
        QString category = getFilterCategory4CodeBlock(block.tag, block.language);
        if (!filterStates4EachCategory[category])
            continue;

        const int row = rowCount();
        insertRow(row);

        auto cursor = block.cursor;

        addCodeBlockLocationToTable(row, block);
        addCodeTypeToTable(row, block);
        addCodeToTable(row, block);
    }
}

void CodeBlocksTableWidget::addCodeBlockLocationToTable(int row, const CodeBlock& block)
{
    QTextCursor cursor = block.cursor;

    int selectionStart = cursor.selectionStart();
    int selectionEnd = cursor.selectionEnd();

    QTextCursor startCursor(cursor.document());
    startCursor.setPosition(selectionStart);

    QTextCursor endCursor(cursor.document());
    endCursor.setPosition(selectionEnd);

    const int startLine = startCursor.blockNumber() + 1;
    const int startPos = startCursor.positionInBlock();
    const int endLine = endCursor.blockNumber() + 1;
    const int endPos = endCursor.positionInBlock();

    QString position;
    if (startLine == endLine)
    {
        position = QString("%1:%2-%3")
            .arg(startLine).arg(startPos).arg(endPos);
    }
    else
    {
        position = QString("%1:%2 - %3:%4")
            .arg(startLine).arg(startPos)
            .arg(endLine).arg(endPos);
    }

    const QString positionToolTip = QString("Code starting in line %1 and column %2, until line %3 and column %4")
        .arg(startLine).arg(startPos)
        .arg(endLine).arg(endPos);

    setCellValue(this, row, Columns::Position, position, positionToolTip);
}

void CodeBlocksTableWidget::addCodeTypeToTable(int row, const CodeBlock& block)
{
    const auto codeType = getDisplayNameFromCodeType(block.tag, block.language);
    setCellValue(this, row, Columns::Type, codeType);
}

void CodeBlocksTableWidget::addCodeToTable(int row, const CodeBlock& block)
{
    const auto codeWithoutTags = getCodeWithoutTags(block);

    // Limit code preview to first line
    int newlinePos = codeWithoutTags.indexOf('\n');
    QString codePreview = newlinePos != -1 ?
        codeWithoutTags.left(newlinePos) + "..." :
        codeWithoutTags;

    setCellValue(this, row, Columns::Code, codePreview);
}


void CodeBlocksTableWidget::onCellClicked(int row, int /*column*/)
{
    if (!textEditor)
        return;

    const auto& blocks = textEditor->getCodeBlocks();
    int blockIndex = 0;

    // Find corresponding block accounting for filtered items
    for (const auto& block : blocks)
    {
        QString category = getFilterCategory4CodeBlock(block.tag, block.language);
        if (!filterStates4EachCategory[category])
            continue;

        if (blockIndex == row)
        {
            // Get cursor position from the block
            int position = block.cursor.selectionStart();

            // Find the code block without tags and select it
            if (auto codeBlock = textEditor->selectEnclosingCodeBlock(position))
            {
                textEditor->setTextCursor(codeBlock->cursor);
                textEditor->setFocus();
            }
            return;
        }
        blockIndex++;
    }
}

void CodeBlocksTableWidget::createFilterMenu()
{
    filterMenu = new QMenu(this);
    filterStates4EachCategory[FILTER_CPP] = true;
    filterStates4EachCategory[FILTER_PYTHON] = true;
    filterStates4EachCategory[FILTER_GENERAL] = true;
    updateFilterMenu();
}

void CodeBlocksTableWidget::updateFilterMenu()
{
    filterMenu->clear();

    for (auto it = filterStates4EachCategory.begin(); it != filterStates4EachCategory.end(); ++it)
    {
        QAction *action = filterMenu->addAction(it.key());
        action->setCheckable(true);
        action->setChecked(it.value());
        connect(action, &QAction::triggered, this, &CodeBlocksTableWidget::applyFilter);
    }

    horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(horizontalHeader(), &QWidget::customContextMenuRequested, [this](const QPoint &pos) {
                filterMenu->exec(horizontalHeader()->mapToGlobal(pos));
    });
}

void CodeBlocksTableWidget::applyFilter()
{
    // Update filter states based on actions
    for (QAction *action: filterMenu->actions())
    {
        filterStates4EachCategory[action->text()] = action->isChecked();
    }
    updateCodeBlocks();
}

void CodeBlocksTableWidget::showEvent(QShowEvent *event)
{
    QTableWidget::showEvent(event);
    updateCodeBlocks();
}

QString CodeBlocksTableWidget::getCodeWithoutTags(const CodeBlock& block) const
{
    if (!textEditor)
        return {};

    // Use selectEnclosingCodeBlock to get code without tags
    if (auto codeBlock = textEditor->selectEnclosingCodeBlock(block.cursor.selectionStart()))
    {
        return codeBlock->cursor.selectedText();
    }
    return QString();
}

bool CodeBlocksTableWidget::event(QEvent *event)
{
    if (QEvent::ToolTip == event->type())
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        QPoint pos = helpEvent->pos();
        int row = rowAt(pos.y());

        // Return if we're in header (row == 0) or outside the table (row == -1)
        if (row <= 0)
        {
            return QTableWidget::event(event);
        }

        // Adjust row index to account for header row
        int blockIndex = row - 1;

        // Show tooltip only if we have valid block index
        const auto codeBlocks = textEditor->getCodeBlocks();
        if (blockIndex >= 0 && blockIndex < codeBlocks.size())
        {
            const CodeBlock& block = codeBlocks[blockIndex];
            QString selectedText = block.cursor.selectedText();

            // Remove opening and closing tags
            QString openTag = QString("[%1]").arg(block.tag);
            QString closeTag = QString("[/%1]").arg(block.tag);
            selectedText.remove(openTag);
            selectedText.remove(closeTag);

            // Convert paragraph separators to newlines for better readability
            selectedText.replace(QChar::ParagraphSeparator, '\n');
            selectedText = selectedText.trimmed(); // Remove any extra whitespace

            QToolTip::showText(helpEvent->globalPos(), selectedText);
            return true;
        }
        else
        {
            QToolTip::hideText();
        }
    }

    return QTableWidget::event(event);
}