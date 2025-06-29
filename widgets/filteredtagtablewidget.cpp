#include <QToolTip>
#include <QAction>
#include <QModelIndexList>
#include "filteredtagtablewidget.h"

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

    QTableWidgetItem* tagItem = item(rowNumber, COLUMN_INDEX_TAG_NAME);
    if (tagItem)
    {
        tagItem->setData(Qt::UserRole, tagName);

        allKnownTags.insert(tagName);
    }

    if (!visibleTags.contains(tagName))
    {
        visibleTags.insert(tagName);
        updateFilterMenu();
    }
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

void FilteredTagTableWidget::updateFilterMenu()
{
    tagFilterMenu->clear();

    for (const QString& tag : allKnownTags)
    {
        QAction* action = new QAction(tag, this);
        action->setCheckable(true);
        action->setChecked(visibleTags.contains(tag)); // nie zawsze true!

        connect(action, &QAction::toggled, this, [this, tag](bool checked) {
            if (checked)
                visibleTags.insert(tag);
            else
                visibleTags.remove(tag);
            applyTagFilter();
        });

        tagFilterMenu->addAction(action);
    }

    if (!visibleTags.isEmpty()) {
        tagFilterMenu->addSeparator();
        QAction* clearAction = new QAction(tr("Wyczyść filtr"), this);
        connect(clearAction, &QAction::triggered, this, [this]() {
            visibleTags.clear();           // tylko aktualnie widoczne
            applyTagFilter();              // ukryje wszystko
            updateFilterMenu();           // zaktualizuj checkboxy
        });
        tagFilterMenu->addAction(clearAction);
    }
}

void FilteredTagTableWidget::applyTagFilter()
{
    const int rowCount = this->rowCount();
    for (int row = 0; row < rowCount; ++row)
    {
        auto* tagItem = item(row, COLUMN_INDEX_TAG_NAME);
        if (!tagItem)
            continue;

        QString tag = tagItem->data(Qt::UserRole).toString();
        setRowHidden(row, !visibleTags.contains(tag));
    }
}

void FilteredTagTableWidget::onCellSingleClicked(int row, int)
{
    QTableWidgetItem *item = this->item(row, 0);
    if (item)
    {
        bool text2NumberSuccess;
        if (const int lineNumber = item->text().toInt(&text2NumberSuccess); text2NumberSuccess)
        {
            emit goToLineClicked(lineNumber);
        }
    }
}

void FilteredTagTableWidget::onHeaderSectionClicked(int logicalIndex)
{
    if (logicalIndex != COLUMN_INDEX_TAG_NAME)
        return;

    int x = horizontalHeader()->sectionPosition(logicalIndex);
    int y = horizontalHeader()->height();

    QPoint globalPos = horizontalHeader()->mapToGlobal(QPoint(x, y));
    tagFilterMenu->exec(globalPos);
}
