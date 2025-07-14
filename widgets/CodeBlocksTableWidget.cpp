#include <QHeaderView>
#include <QAction>
#include <QTextBlock>
#include <QToolTip>
#include "widgets/CodeBlocksTableWidget.h"
#include "codeeditor.h"
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
    void addCode(QTableWidget *table, int row, const QString& codeWithoutTags)
    {
        constexpr int column = 2;

        // Limit code preview to first line
        int newlinePos = codeWithoutTags.indexOf('\n');
        QString codePreview = newlinePos != -1 ?
            codeWithoutTags.left(newlinePos) + "..." :
            codeWithoutTags;

        // auto* codeItem = new QTableWidgetItem(codePreview);
        auto* codeItem = table->item(row, column);
        if (!codeItem)
        {
            codeItem = new QTableWidgetItem(codePreview);
            table->setItem(row, column, codeItem);
        }

        // Store full code in item data for tooltip
        codeItem->setData(Qt::UserRole, codeWithoutTags);
        codeItem->setFlags(codeItem->flags() & ~Qt::ItemIsEditable);

        table->setItem(row, Code, codeItem);
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

        addCodeBlockLocation(row, block);

        auto *typeItem = new QTableWidgetItem(getDisplayNameFromCodeType(block.tag, block.language));
        setItem(row, Type, typeItem);

        addCodeToTable(row, block);
    }
}
void CodeBlocksTableWidget::addCodeBlockLocation(int row, const CodeBlock& block)
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

    constexpr int column = 0;

    auto *tableItem = item(row, column);
    if (!tableItem)
    {
        tableItem = new QTableWidgetItem;
        setItem(row, column, tableItem);
    }

    tableItem->setData(Qt::DisplayRole, position);
    tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
    tableItem->setToolTip(positionToolTip); // TODO: Błąd
}
void CodeBlocksTableWidget::addCodeToTable(int row, const CodeBlock& block)
{
    constexpr int column = 2;

    const auto codeWithoutTags = getCodeWithoutTags(block);

    // Limit code preview to first line
    int newlinePos = codeWithoutTags.indexOf('\n');
    QString codePreview = newlinePos != -1 ?
        codeWithoutTags.left(newlinePos) + "..." :
        codeWithoutTags;

    auto* codeItem = item(row, column);
    if (!codeItem)
    {
        codeItem = new QTableWidgetItem(codePreview);
        setItem(row, column, codeItem);
    }

    // Store full code in item data for tooltip
    codeItem->setData(Qt::UserRole, codeWithoutTags);
    codeItem->setFlags(codeItem->flags() & ~Qt::ItemIsEditable);
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

bool CodeBlocksTableWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QModelIndex index = indexAt(helpEvent->pos());

        // Check if mouse is above cell in column Code
        if (index.isValid() && index.column() == Code) {
            QTableWidgetItem* item = this->item(index.row(), Code);
            if (item) {
                QString fullCode = item->data(Qt::UserRole).toString();
                if (!fullCode.isEmpty()) {
                    QString htmlCode = fullCode.toHtmlEscaped()
                                            .replace("\n", "<br>")
                                            .replace(" ", "&nbsp;"); // zachowaj spacje
                    QToolTip::showText(helpEvent->globalPos(),
                                     "<pre style='white-space: pre-wrap;'>" + htmlCode + "</pre>");
                    return true;
                }
            }
        }
        QToolTip::hideText();
        return true;
    }
    return QTableWidget::event(event);
}
