#include <QHeaderView>
#include <QAction>
#include <QTextBlock>
#include "widgets/CodeBlocksTableWidget.h"
#include "codeeditor.h"

namespace
{
    void addPosition(QTableWidget *table, int row, QTextCursor cursor)
    {
        const int startLine = cursor.blockNumber() + 1;
        const int endLine = cursor.document()->findBlock(cursor.selectionEnd()).blockNumber() + 1;
        const int startPos = cursor.positionInBlock();
        const int endPos = cursor.document()->findBlock(cursor.selectionEnd()).length();

        const QString position = QString("%1:%2 - %3:%4")
            .arg(startLine).arg(startPos)
            .arg(endLine).arg(endPos);
        const QString positionToolTip = QString("Code starting in line %1 and column %2, until line %3 and column %4")
            .arg(startLine).arg(startPos)
            .arg(endLine).arg(endPos);

        constexpr int column = 0;

        auto *tableItem = table->item(row, column);
        if (!tableItem)
        {
            tableItem = new QTableWidgetItem;
            table->setItem(row, column, tableItem);
        }

        tableItem->setData(Qt::DisplayRole, position);
        tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
        tableItem->setToolTip(positionToolTip);
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
    setHorizontalHeaderLabels({"Position", "Type"});
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    horizontalHeader()->setSectionResizeMode(Position, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(Type, QHeaderView::Stretch);
    verticalHeader()->hide();
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

        int row = rowCount();
        insertRow(row);

        auto cursor = block.cursor;

        addPosition(this, row, block.cursor);

        auto *typeItem = new QTableWidgetItem(getDisplayNameFromCodeType(block.tag, block.language));
        setItem(row, Type, typeItem);
    }
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
