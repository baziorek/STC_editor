#include <QTextBlock>
#include <QHeaderView>
#include <QToolTip>
#include <QAction>
#include <QMenu>
#include "FilteredTagTableWidget.h"
#include "codeeditor.h"


namespace
{
enum ColumnIndices
{
    COLUMN_LINE = 0,
    COLUMN_TAG,
    COLUMN_TEXT,
    COLUMN_COUNT
};
}

struct FilteredTagTableWidget::HeaderInfo
{
    QTextCursor startingTagCursor;
    QString tagName;
    QString textInside;
    int startPos;
    int endPos;
};


FilteredTagTableWidget::FilteredTagTableWidget(QWidget* parent)
    : QTableWidget(parent),
    tagFilterMenu(new QMenu(this))
{
    setColumnCount(COLUMN_COUNT);
    setHorizontalHeaderLabels({ "Line", "Tag ▾", "Text" });
    horizontalHeader()->setSectionResizeMode(COLUMN_LINE, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_TAG, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_TEXT, QHeaderView::Stretch);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::NoSelection); // ❗️Disable default selection
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);
    setAlternatingRowColors(true);

    connect(this, &QTableWidget::cellClicked, this, &FilteredTagTableWidget::onCellSingleClicked);
    connect(horizontalHeader(), &QHeaderView::sectionClicked, this, &FilteredTagTableWidget::onHeaderSectionClicked);
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
    QTableWidget::showEvent(event);
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

void FilteredTagTableWidget::clearHeaderTable()
{
    setRowCount(0);
    tagVisibility.clear();
    updateFilterMenu();
}

void FilteredTagTableWidget::insertOrUpdateHeader(const HeaderInfo& info)
{
    const int lineDisplay = info.startingTagCursor.blockNumber() + 1;

    int row = 0;
    for (; row < rowCount(); ++row)
    {
        auto* item = this->item(row, COLUMN_LINE);
        if (item && item->text().toInt() > lineDisplay)
            break;
    }
    insertRow(row);

    auto* lineItem = new QTableWidgetItem(QString::number(lineDisplay));
    lineItem->setFlags(lineItem->flags() & ~Qt::ItemIsEditable);

    auto* tagItem = new QTableWidgetItem(info.tagName);
    tagItem->setFlags(tagItem->flags() & ~Qt::ItemIsEditable);
    tagItem->setData(Qt::UserRole, info.tagName);

    auto* textItem = new QTableWidgetItem(info.textInside);
    textItem->setFlags(textItem->flags() & ~Qt::ItemIsEditable);
    textItem->setToolTip(info.textInside);

    setItem(row, COLUMN_LINE, lineItem);
    setItem(row, COLUMN_TAG, tagItem);
    setItem(row, COLUMN_TEXT, textItem);

    if (!tagVisibility.contains(info.tagName))
    {
        tagVisibility[info.tagName] = true;
        updateFilterMenu();
    }
}

void FilteredTagTableWidget::onTextChanged(int pos, int /*charsRemoved*/, int /*charsAdded*/)
{
    QTextBlock changedBlock = textEditor->document()->findBlock(pos);
    if (!changedBlock.isValid())
        return;

    const int changedLine = changedBlock.blockNumber();

    // Reanalyse header presence in this block
    QTextCursor cursor(changedBlock);
    QString blockText = changedBlock.text();
    QRegularExpressionMatch match = headerRegex().match(blockText);

    // Remove any old header in that line
    cachedHeaders.erase(std::remove_if(cachedHeaders.begin(), cachedHeaders.end(),
                                       [changedLine](const HeaderInfo& h) {
                                           return h.startingTagCursor.block().blockNumber() == changedLine;
                                       }), cachedHeaders.end());

    if (match.hasMatch())
    {
        const int absolutePos = changedBlock.position() + match.capturedStart();
        if (!textEditor->isInsideCode(absolutePos))
        {
            HeaderInfo info {
                .startingTagCursor = cursor,
                .tagName = match.captured(1),
                .textInside = match.captured(2),
                .startPos = absolutePos,
                .endPos = static_cast<int>(absolutePos + match.capturedLength())
            };
            cachedHeaders.append(info);
        }
    }

    // Sort after potential update
    std::sort(cachedHeaders.begin(), cachedHeaders.end(), [](const HeaderInfo& a, const HeaderInfo& b) {
        return a.startingTagCursor.block().blockNumber() < b.startingTagCursor.block().blockNumber();
    });

    refreshHeaderTable();
}

void FilteredTagTableWidget::refreshHeaderTable()
{
    setRowCount(0);

    for (const auto& info : cachedHeaders)
    {
        insertOrUpdateHeader(info);
    }

    applyTagFilter();

    highlightedRow = -1;
    highlightCurrentTagInContextTable();
}


void FilteredTagTableWidget::applyTagFilter()
{
    const int totalRows = rowCount();
    for (int row = 0; row < totalRows; ++row)
    {
        QTableWidgetItem* tagItem = item(row, COLUMN_TAG);
        if (!tagItem)
            continue;

        QString tag = tagItem->data(Qt::UserRole).toString();
        bool visible = tagVisibility.value(tag, true);
        setRowHidden(row, !visible);
    }
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

void FilteredTagTableWidget::onCellSingleClicked(int row, int)
{
    if (auto* item = this->item(row, COLUMN_LINE))
    {
        bool ok = false;
        int line = item->text().toInt(&ok);
        if (ok)
            emit goToLineClicked(line);
    }
}

void FilteredTagTableWidget::onHeaderSectionClicked(int logicalIndex)
{
    if (logicalIndex != COLUMN_TAG)
        return;

    int x = horizontalHeader()->sectionPosition(logicalIndex);
    int y = horizontalHeader()->height();
    QPoint globalPos = horizontalHeader()->mapToGlobal(QPoint(x, y));
    tagFilterMenu->exec(globalPos);
}

/*
 * ─────────────────────────────────────────────────────────────────────────────
 * Known Issue & Rationale for Custom Highlighting:
 *
 * In certain scenarios — such as inserting characters (especially special ones
 * like @, !, ?, etc.) or newlines near tag boundaries — the built-in Qt
 * selection system (via selectRow / selectionModel) may fail to visually
 * update the current context row.
 *
 * This behavior is caused by the way QPlainTextEdit and QTextDocument manage
 * text blocks and cursor updates:
 *
 *   - When text is edited, especially near QTextBlock boundaries,
 *     the internal layout engine may temporarily place the cursor at a block
 *     not yet fully updated.
 *
 *   - Even if our context detection logic returns the correct row, calling
 *     selectRow(bestRow) will not result in a visible highlight unless the
 *     table has focus or the selection model deems the selection “active”.
 *
 *   - Additionally, selection may be auto-cleared by Qt focus changes or
 *     other UI events.
 *
 * Therefore, we implement our own visual row highlighting by directly
 * modifying item background/foreground colors instead of relying on the
 * built-in selection system.
 *
 * This avoids visual flicker and ensures context highlighting always reflects
 * the current cursor position in the editor — regardless of focus state or
 * Qt internals.
 *
 * Note: standard row selection (selectRow/clearSelection) is still disabled
 * to prevent duplicate visual indicators.
 * ─────────────────────────────────────────────────────────────────────────────
 */
void FilteredTagTableWidget::highlightCurrentTagInContextTable()
{
    if (!textEditor || isHidden() || cachedHeaders.isEmpty())
        return;

    auto bestRow = findHeaderForCursor(textEditor->textCursor());
    if (bestRow >= 0 && bestRow != highlightedRow)
    {
        clearHighlightedRow();
        highlightRow(bestRow);
        scrollToItem(item(bestRow, 0), QAbstractItemView::PositionAtCenter);
    }
}
int FilteredTagTableWidget::findHeaderForCursor(const QTextCursor &cursor) const
{
    const int cursorBlock = cursor.block().blockNumber();

    for (int row = 0; row < cachedHeaders.size(); ++row)
    {
        int startLine = cachedHeaders[row].startingTagCursor.block().blockNumber();
        int endLine = (row + 1 < cachedHeaders.size())
                          ? cachedHeaders[row + 1].startingTagCursor.block().blockNumber()
                          : std::numeric_limits<int>::max(); // last header goes to EOF

        if (cursorBlock >= startLine && cursorBlock < endLine)
        {
            return row;
        }
    }
    return -1;
}

void FilteredTagTableWidget::highlightRow(int row)
{
    highlightedRow = row;
    const QColor backgroundColor = palette().highlight().color();
    const QColor textColor = palette().highlightedText().color();

    for (int col = 0; col < columnCount(); ++col)
    {
        QTableWidgetItem* item = this->item(row, col);
        if (item)
        {
            item->setBackground(backgroundColor);
            item->setForeground(textColor);
        }
    }
}

void FilteredTagTableWidget::clearHighlightedRow()
{
    if (highlightedRow < 0 || highlightedRow >= rowCount())
        return;

    const QColor backgroundColor = palette().base().color();
    const QColor textColor = palette().text().color();

    for (int col = 0; col < columnCount(); ++col)
    {
        QTableWidgetItem* item = this->item(highlightedRow, col);
        if (item)
        {
            item->setBackground(backgroundColor);
            item->setForeground(textColor);
        }
    }

    highlightedRow = -1;
}
