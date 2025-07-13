#include <QToolTip>
#include <QAction>
#include "filteredtagtablewidget.h"
#include "codeeditor.h"


namespace
{
enum ColumnIndices
{
    COLUMN_INDEX_LINE_NUMBER = 0,
    COLUMN_INDEX_TAG_NAME = 1,
    COLUMN_INDEX_CONTENT = 2,
    COLUMN_INDEX_COLUMN_COUNT = 3
};

struct TextInsideTags
{
    QString tag, text;
};

QRegularExpression& allContextTagsRegex()
{
    // static QRegularExpression re(
    //     R"(\[(h[1-6]|div|pkt|csv|cpp|py|code)(?:\s+[^\]]+)?\](.*?)\[/\1\])",
    //     QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression re(
        R"(\[(h[1-6]|div|pkt|csv)(?:\s+[^\]]+)?\](.*?)\[/\1\])", // no code here
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    return re;
}

std::map<int, TextInsideTags> findTagMatches(const QRegularExpression& regex, const QString& text)
{
    std::map<int, TextInsideTags> textPerLine;
    for (QRegularExpressionMatchIterator matches = regex.globalMatch(text); matches.hasNext(); )
    {
        // TODO: Can we make finding line number more optimal?
        QRegularExpressionMatch match = matches.next();
        auto lineNumber = text.left(match.capturedStart(0)).count('\n') + 1;
        textPerLine.insert({lineNumber, TextInsideTags{match.captured(1), match.captured(2)}});
    }
    return textPerLine;
}

void updateContextTable(FilteredTagTableWidget* table, auto& taggedTextLinePositions)
{
    table->setRowCount(taggedTextLinePositions.size());

    unsigned rowNumber = 0;
    for (const auto& [lineNumber, tagAndText] : taggedTextLinePositions)
    {
        const auto& [tag, text] = tagAndText;
        table->insertRow(rowNumber,
                         /*lineNumber=*/lineNumber,
                         /*tagName=*/tag,
                         /*tagText=*/text);

        ++rowNumber;
    }
}

int tagLevel(const QString& tag)
{
    QString lower = tag.toLower();
    if (lower.startsWith("h"))
        return lower.mid(1).toInt(); // h1 -> 1, h2 -> 2
    if (lower == "div")
        return 0;
    if (lower == "pkt")
        return -1;
    if (lower == "csv")
        return -2;
    if (lower == "cpp" || lower == "py" || lower == "code")
        return -3;
    return -10; // fallback
}
} // namespace


FilteredTagTableWidget::FilteredTagTableWidget(QWidget* parent)
    : QTableWidget(parent), tagFilterMenu(new QMenu(this))
{
    setColumnCount(COLUMN_INDEX_COLUMN_COUNT);
    setHorizontalHeaderLabels({"Line", "Tag ▾", "Text"});
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_LINE_NUMBER, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_TAG_NAME, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_CONTENT, QHeaderView::Stretch);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);

    setAlternatingRowColors(true);

    connect(this, &QTableWidget::cellClicked, this, &FilteredTagTableWidget::onCellSingleClicked);
    connect(horizontalHeader(), &QHeaderView::sectionClicked, this, &FilteredTagTableWidget::onHeaderSectionClicked);
}

void FilteredTagTableWidget::showEvent(QShowEvent* event)
{
    QTableWidget::showEvent(event);
    if (isVisible())
    {
        onUpdateContextRequested();
    }
}


void FilteredTagTableWidget::insertRow(int rowNumber, int lineNumber, QString tagName, QString textInsideTag)
{
    insertText2Cell(rowNumber, COLUMN_INDEX_LINE_NUMBER, QString::number(lineNumber));
    insertText2Cell(rowNumber, COLUMN_INDEX_TAG_NAME, tagName);
    insertText2Cell(rowNumber, COLUMN_INDEX_CONTENT, textInsideTag);

    if (QTableWidgetItem* tagItem = item(rowNumber, COLUMN_INDEX_TAG_NAME))
    {
        tagItem->setData(Qt::UserRole, tagName);
    }

    if (! tagVisibility.contains(tagName))
    {
        tagVisibility[tagName] = true; // visible by default
        updateFilterMenu();
    }

    applyTagFilter();
}

void FilteredTagTableWidget::insertText2Cell(int row, int column, const QString &text)
{
    if (auto* cell = item(row, column); cell == nullptr)
    {
        cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        setItem(row, column, cell);
    }
    else
    {
        item(row, column)->setText(text);
    }

    if (column == COLUMN_INDEX_CONTENT)
    {
        item(row, column)->setToolTip(text);
    }
}

void FilteredTagTableWidget::clearTags()
{
    tagVisibility.clear();
    updateFilterMenu();
}

void FilteredTagTableWidget::onUpdateContextRequested()
{
    if (isHidden())
        return;

    const auto text = textEditor->toPlainText();

    auto taggedTextLinePositions = findTagMatches(allContextTagsRegex(), text);

    updateContextTable(this, taggedTextLinePositions);

    highlightCurrentTagInContextTable();
}

void FilteredTagTableWidget::updateFilterMenu()
{
    tagFilterMenu->clear();

    for (auto it = tagVisibility.begin(); it != tagVisibility.end(); ++it)
    {
        const QString& tag = it.key();
        bool isVisible = it.value();

        QAction* action = new QAction(tag, this);
        action->setCheckable(true);
        action->setChecked(isVisible);

        connect(action, &QAction::toggled, this, [this, tag](bool checked) {
            tagVisibility[tag] = checked;
            applyTagFilter();
        });

        tagFilterMenu->addAction(action);
    }

    if (!tagVisibility.isEmpty())
    {
        tagFilterMenu->addSeparator();

        QAction* deselectAllAction = new QAction(tr("Deselect all"), this);
        connect(deselectAllAction, &QAction::triggered, this, [this]() {
            for (auto& value : tagVisibility)
                value = false;
            applyTagFilter();
            updateFilterMenu();
        });
        tagFilterMenu->addAction(deselectAllAction);

        QAction* selectAllAction = new QAction(tr("Select all"), this);
        connect(selectAllAction, &QAction::triggered, this, [this]() {
            for (auto& value : tagVisibility)
                value = true;
            applyTagFilter();
            updateFilterMenu();
        });
        tagFilterMenu->addAction(selectAllAction);
    }
}

void FilteredTagTableWidget::applyTagFilter()
{
    const int totalRows = rowCount();
    for (int row = 0; row < totalRows; ++row)
    {
        auto* tagItem = item(row, COLUMN_INDEX_TAG_NAME);
        if (!tagItem)
            continue;

        QString tag = tagItem->data(Qt::UserRole).toString();
        bool visible = tagVisibility.value(tag, true);
        setRowHidden(row, !visible);
    }
}

void FilteredTagTableWidget::onCellSingleClicked(int row, int)
{
    QTableWidgetItem* item = this->item(row, 0);
    if (!item)
        return;

    bool ok;
    if (int line = item->text().toInt(&ok); ok)
    {
        emit goToLineClicked(line);
    }
}

void FilteredTagTableWidget::onHeaderSectionClicked(int logicalIndex)
{
    if (logicalIndex != COLUMN_INDEX_TAG_NAME)
    {
        return;
    }

    int x = horizontalHeader()->sectionPosition(logicalIndex);
    int y = horizontalHeader()->height();
    QPoint globalPos = horizontalHeader()->mapToGlobal(QPoint(x, y));
    tagFilterMenu->exec(globalPos);
}

void FilteredTagTableWidget::highlightCurrentTagInContextTable()
{
    if (isHidden())
        return;

    const int cursorPos = textEditor->textCursor().position();
    const QString text = textEditor->toPlainText();

    struct TagEntry
    {
        int row;
        int start;
        int end;
        QString tag;
        int level;
    };

    QList<TagEntry> tags;

    for (int row = 0; row < rowCount(); ++row)
    {
        auto* lineItem = item(row, 0);
        if (!lineItem)
            continue;

        bool ok = false;
        int lineNumber = lineItem->text().toInt(&ok);
        if (!ok)
            continue;

        int startOffset = 0;
        for (int i = 1; i < lineNumber; ++i)
            startOffset = text.indexOf('\n', startOffset) + 1;

        QRegularExpressionMatch match = allContextTagsRegex().match(text, startOffset);
        if (match.hasMatch())
        {
            const QString tag = match.captured(1).toLower();
            const int start = match.capturedStart();
            const int end = match.capturedEnd();
            const int level = tagLevel(tag);
            tags.append({ row, start, end, tag, level });
        }
    }

    // Sort by start position
    std::sort(tags.begin(), tags.end(), [](const TagEntry& a, const TagEntry& b) {
        return a.start < b.start;
    });

    int bestRow = -1;
    int bestLevel = std::numeric_limits<int>::min();

    for (int i = 0; i < tags.size(); ++i)
    {
        const auto& tag = tags[i];

        if (tag.start > cursorPos)
            break;

        // Range tags - active only if the cursor is within their range
        if (tag.tag == "div" || tag.tag == "pkt" || tag.tag == "csv")
        {
            if (cursorPos <= tag.end)
            {
                bestRow = tag.row;
                bestLevel = tag.level;
            }
        }
        else if (tag.tag.startsWith("h"))
        {
            // Header remains active until overridden
            bestRow = tag.row;
            bestLevel = tag.level;
        }
    }

    clearSelection();

    if (bestRow >= 0)
    {
        selectRow(bestRow);
        scrollToItem(item(bestRow, 0), QAbstractItemView::PositionAtCenter);
    }
}
