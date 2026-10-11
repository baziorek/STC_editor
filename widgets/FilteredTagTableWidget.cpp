#include <QTextBlock>
#include <QHeaderView>
#include <QToolTip>
#include <QAction>
#include <QMenu>
#include <algorithm>
#include <limits>
#include "FilteredTagTableWidget.h"
#include "CodeEditor.h"
#include "utils/HeaderNesting.h"


namespace
{
enum ColumnIndices
{
    COLUMN_LINE = 0,
    COLUMN_TAG,
    COLUMN_TEXT,       // the tree: the header is indented under its parent (see setTreePosition)
    COLUMN_COUNT
};

/// h1 => 1 ... h6 => 6
int levelOf(const QString& tagName)
{
    return std::clamp(tagName.mid(1).toInt(), 1, 6);
}
}


FilteredTagTableWidget::FilteredTagTableWidget(QWidget* parent)
    : QTreeWidget(parent),
    tagFilterMenu(new QMenu(this))
{
    setColumnCount(COLUMN_COUNT);
    setHeaderLabels({ tr("Line"), tr("Tag ▾"), tr("Text") });
    // the tree (indentation, expander) is drawn in the text column, which is the last one: the line and the tag take
    // what they need and the text takes the rest of the width
    setTreePosition(COLUMN_TEXT);
    header()->setSectionResizeMode(COLUMN_LINE, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(COLUMN_TAG, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(COLUMN_TEXT, QHeaderView::Stretch);
    // a header of a tree view is not clickable unless asked for: the click on "Tag" opens the filter
    header()->setSectionsClickable(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::NoSelection); // ❗️Disable default selection
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);

    connect(this, &QTreeWidget::itemClicked, this, &FilteredTagTableWidget::onItemClicked);
    connect(header(), &QHeaderView::sectionClicked, this, &FilteredTagTableWidget::onHeaderSectionClicked);
}

FilteredTagTableWidget::~FilteredTagTableWidget() = default;

void FilteredTagTableWidget::setTextEditor(CodeEditor* newTextEditor)
{
    if (textEditor)
    {
        disconnect(textEditor->document(), &QTextDocument::contentsChange, this, &FilteredTagTableWidget::onTextChanged);
        disconnect(textEditor, &CodeEditor::cursorPositionChanged, this, &FilteredTagTableWidget::highlightCurrentTagInContextTable);
    }

    textEditor = newTextEditor;

    if (textEditor)
    {
        connect(textEditor->document(), &QTextDocument::contentsChange, this, &FilteredTagTableWidget::onTextChanged);
        connect(textEditor, &CodeEditor::cursorPositionChanged, this, &FilteredTagTableWidget::highlightCurrentTagInContextTable);
        rebuildAllHeaders();
    }
}

void FilteredTagTableWidget::showEvent(QShowEvent* event)
{
    QTreeWidget::showEvent(event);
    if (isVisible())
        rebuildAllHeaders();
}

QRegularExpression FilteredTagTableWidget::headerRegex()
{
    static QRegularExpression regex(R"(\[(h[1-6])(?:\s+[^\]]+)?\](.*?)\[/\1\])",
                                    QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    return regex;
}

void FilteredTagTableWidget::rebuildAllHeaders()
{
    cachedHeaders.clear();
    clearHeaderTable();

    if (!textEditor || textEditor->toPlainText().isEmpty())
        return;

    const QString text = textEditor->toPlainText();
    const QRegularExpression& re = headerRegex();
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        const int start = match.capturedStart();
        const int end = match.capturedEnd();

        if (textEditor->isInsideCode(start))
            continue;

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(start);

        HeaderInfo info {
            .startingTagCursor = cursor,
            .tagName = match.captured(1),
            .textInside = match.captured(2),
            .startPos = start,
            .endPos = end
        };

        cachedHeaders.append(info);
    }

    // Sort headers by current block number
    std::sort(cachedHeaders.begin(), cachedHeaders.end(), [](const HeaderInfo& a, const HeaderInfo& b) {
        return a.startingTagCursor.block().blockNumber() < b.startingTagCursor.block().blockNumber();
    });

    refreshHeaderTable();
}

void FilteredTagTableWidget::clear()
{
    cachedHeaders.clear();
    clearHeaderTable();
    highlightedHeader = -1;
}

void FilteredTagTableWidget::clearHeaderTable()
{
    QTreeWidget::clear();
    itemOfHeader.clear();
    tagVisibility.clear();
    updateFilterMenu();
}

void FilteredTagTableWidget::onTextChanged(int pos, int charsRemoved, int charsAdded)
{
    // If we don't have a text editor or document is empty, clear the headers
    if (!textEditor || !textEditor->document() || textEditor->document()->blockCount() == 0)
    {
        clear();
        return;
    }

    // Get the block where the change occurred
    QTextCursor cursor(textEditor->document());
    cursor.setPosition(pos);
    QTextBlock changedBlock = cursor.block();

    // If there's no actual change, do nothing
    if (charsRemoved == 0 && charsAdded == 0)
        return;

    // Calculate the position offset caused by this change
    const int offset = charsAdded - charsRemoved;

    // Update positions of existing headers that come after the change
    for (auto& header : cachedHeaders)
    {
        // If header is after the change, update its position
        if (header.startPos >= pos + charsRemoved)
        {
            header.startPos += offset;
            header.endPos += offset;
            header.startingTagCursor.setPosition(header.startPos);
        }
        // If change is inside this header, mark it for re-analysis
        else if (header.startPos <= pos && header.endPos >= pos + charsRemoved)
        {
            // Mark for re-analysis
            header.startPos = -1;
        }
    }

    // Remove headers that need re-analysis
    cachedHeaders.erase(
        std::remove_if(cachedHeaders.begin(), cachedHeaders.end(),
                       [](const HeaderInfo& h) { return h.startPos == -1; }),
        cachedHeaders.end());

    // Find the range of blocks that might have been affected by this change
    QTextBlock block = textEditor->document()->findBlock(pos);
    QTextBlock endBlock = textEditor->document()->findBlock(pos + charsAdded);

    // Go back to the first block that might have been affected
    while (block.isValid() && block != endBlock)
    {
        block = block.previous();
        if (!block.isValid())
            break;
    }
    if (!block.isValid())
        block = textEditor->document()->begin();

    // Process all affected blocks
    while (block.isValid() && block != endBlock.next())
    {
        const QString blockText = block.text();
        QRegularExpressionMatchIterator it = headerRegex().globalMatch(blockText);

        // Remove all headers from this block (we'll re-add the current ones)
        cachedHeaders.erase(
            std::remove_if(cachedHeaders.begin(), cachedHeaders.end(),
                           [block](const HeaderInfo& h) {
                               return h.startingTagCursor.block() == block;
                           }),
            cachedHeaders.end());

        // Add current headers from this block
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            const int start = block.position() + match.capturedStart();

            if (!textEditor->isInsideCode(start))
            {
                QTextCursor cursor(block);
                cursor.setPosition(start);

                HeaderInfo info{
                    .startingTagCursor = cursor,
                    .tagName = match.captured(1),
                    .textInside = match.captured(2).trimmed(),
                    .startPos = start,
                    .endPos = static_cast<int>(start + match.capturedLength())
                };

                cachedHeaders.append(info);
            }
        }

        block = block.next();
    }

    // Keep headers sorted by position
    std::sort(cachedHeaders.begin(), cachedHeaders.end(),
              [](const HeaderInfo& a, const HeaderInfo& b) {
                  return a.startPos < b.startPos;
              });

    // Update the view
    refreshHeaderTable();
}

void FilteredTagTableWidget::refreshHeaderTable()
{
    bool newTagAppeared = false;
    for (const auto& info : cachedHeaders)
    {
        if (!tagVisibility.contains(info.tagName))
        {
            tagVisibility[info.tagName] = true;
            newTagAppeared = true;
        }
    }
    if (newTagAppeared)
        updateFilterMenu();

    rebuildTree();
    highlightCurrentTagInContextTable();
}

void FilteredTagTableWidget::rebuildTree()
{
    QTreeWidget::clear();
    itemOfHeader.clear();
    highlightedHeader = -1;

    // only the headers whose tags are shown take part in the tree
    QList<int> shownHeaders;
    std::vector<int> levels;
    for (int index = 0; index < cachedHeaders.size(); ++index)
    {
        if (tagVisibility.value(cachedHeaders[index].tagName, true))
        {
            shownHeaders.append(index);
            levels.push_back(levelOf(cachedHeaders[index].tagName));
        }
    }

    const std::vector<int> parents = headerParents(levels);
    QList<QTreeWidgetItem*> items;

    for (int position = 0; position < shownHeaders.size(); ++position)
    {
        const int index = shownHeaders[position];
        const HeaderInfo& info = cachedHeaders[index];

        auto* item = new QTreeWidgetItem();
        item->setText(COLUMN_LINE, QString::number(info.startingTagCursor.blockNumber() + 1));
        item->setText(COLUMN_TAG, info.tagName);
        item->setText(COLUMN_TEXT, info.textInside);
        item->setToolTip(COLUMN_TEXT, info.textInside);

        const int parent = parents[position];
        if (parent < 0)
            addTopLevelItem(item);
        else
            items[parent]->addChild(item);

        items.append(item);
        itemOfHeader.insert(index, item);
    }

    expandAll();
}

void FilteredTagTableWidget::applyTagFilter()
{
    rebuildTree();
    highlightCurrentTagInContextTable();
}

void FilteredTagTableWidget::updateFilterMenu()
{
    tagFilterMenu->clear();

    for (auto it = tagVisibility.begin(); it != tagVisibility.end(); ++it)
    {
        QAction* action = new QAction(it.key(), this);
        action->setCheckable(true);
        action->setChecked(it.value());

        connect(action, &QAction::toggled, this, [this, tag = it.key()](bool checked) {
            tagVisibility[tag] = checked;
            applyTagFilter();
        });

        tagFilterMenu->addAction(action);
    }

    if (!tagVisibility.isEmpty())
    {
        tagFilterMenu->addSeparator();

        QAction* deselectAll = new QAction(tr("Deselect all"), this);
        connect(deselectAll, &QAction::triggered, this, [this]() {
            for (auto& val : tagVisibility)
                val = false;
            applyTagFilter(); updateFilterMenu();
        });
        tagFilterMenu->addAction(deselectAll);

        QAction* selectAll = new QAction(tr("Select all"), this);
        connect(selectAll, &QAction::triggered, this, [this]() {
            for (auto& val : tagVisibility)
                val = true;
            applyTagFilter(); updateFilterMenu();
        });
        tagFilterMenu->addAction(selectAll);
    }
}

void FilteredTagTableWidget::onItemClicked(QTreeWidgetItem* item, int)
{
    if (!item)
        return;

    bool ok = false;
    int line = item->text(COLUMN_LINE).toInt(&ok);
    if (ok)
        emit goToLineClicked(line);
}

void FilteredTagTableWidget::onHeaderSectionClicked(int logicalIndex)
{
    if (logicalIndex != COLUMN_TAG)
        return;

    int x = header()->sectionPosition(logicalIndex);
    int y = header()->height();
    QPoint globalPos = header()->mapToGlobal(QPoint(x, y));
    tagFilterMenu->exec(globalPos);
}

/*
 * The current header is shown with the colours of a selection, drawn by hand: the built-in selection of the view does
 * not show up reliably while the text is being edited (the cursor is still at a block which is not fully updated),
 * and it is cleared by focus changes. Standard selection stays disabled, so there is one indicator only.
 */
void FilteredTagTableWidget::highlightCurrentTagInContextTable()
{
    if (!textEditor || isHidden() || cachedHeaders.isEmpty())
        return;

    // the header the cursor is in; if its tag is switched off, the closest shown header above it
    int headerIndex = findHeaderForCursor(textEditor->textCursor());
    while (headerIndex >= 0 && !itemOfHeader.contains(headerIndex))
        --headerIndex;

    if (headerIndex >= 0 && headerIndex != highlightedHeader)
    {
        clearHighlightedHeader();
        highlightHeader(headerIndex);
        scrollToItem(itemOfHeader.value(headerIndex), QAbstractItemView::PositionAtCenter);
    }
}

int FilteredTagTableWidget::findHeaderForCursor(const QTextCursor &cursor) const
{
    const int cursorBlock = cursor.block().blockNumber();

    for (int index = 0; index < cachedHeaders.size(); ++index)
    {
        int startLine = cachedHeaders[index].startingTagCursor.block().blockNumber();
        int endLine = (index + 1 < cachedHeaders.size())
                          ? cachedHeaders[index + 1].startingTagCursor.block().blockNumber()
                          : std::numeric_limits<int>::max(); // last header goes to EOF

        if (cursorBlock >= startLine && cursorBlock < endLine)
        {
            return index;
        }
    }
    return -1;
}

void FilteredTagTableWidget::highlightHeader(int headerIndex)
{
    highlightedHeader = headerIndex;

    QTreeWidgetItem* item = itemOfHeader.value(headerIndex, nullptr);
    if (!item)
        return;

    for (int col = 0; col < columnCount(); ++col)
    {
        item->setBackground(col, palette().highlight());
        item->setForeground(col, palette().highlightedText());
    }
}

void FilteredTagTableWidget::clearHighlightedHeader()
{
    if (highlightedHeader < 0)
        return;

    // back to what the view paints by itself (alternating colours included)
    if (QTreeWidgetItem* item = itemOfHeader.value(highlightedHeader, nullptr))
    {
        for (int col = 0; col < columnCount(); ++col)
        {
            item->setData(col, Qt::BackgroundRole, QVariant());
            item->setData(col, Qt::ForegroundRole, QVariant());
        }
    }

    highlightedHeader = -1;
}
