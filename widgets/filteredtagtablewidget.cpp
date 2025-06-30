#include "filteredtagtablewidget.h"
#include <QToolTip>
#include <QAction>


enum ColumnIndices
{
    COLUMN_INDEX_LINE_NUMBER = 0,
    COLUMN_INDEX_TAG_NAME = 1,
    COLUMN_INDEX_CONTENT = 2,
    COLUMN_INDEX_COLUMN_COUNT = 3
};


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

    connect(this, &QTableWidget::cellClicked, this, &FilteredTagTableWidget::onCellSingleClicked);
    connect(horizontalHeader(), &QHeaderView::sectionClicked, this, &FilteredTagTableWidget::onHeaderSectionClicked);
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
