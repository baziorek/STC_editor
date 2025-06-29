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
    setHorizontalHeaderLabels({"Line", "Tag", "Text"});
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_LINE_NUMBER, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_TAG_NAME, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(COLUMN_INDEX_CONTENT, QHeaderView::Stretch);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->setVisible(false);

    connect(this, &QTableWidget::cellClicked, this, &FilteredTagTableWidget::onCellSingleClicked);
}

void FilteredTagTableWidget::setData(const TaggedData& data)
{
    allData = data;
    setRowCount(data.size());

    QSet<QString> allTags;
    int row = 0;
    for (const auto& entry : data)
    {
        insertRow(row++, entry);
        allTags.insert(std::get<1>(entry));
    }
    visibleTags = allTags;
    updateFilterMenu();
    applyTagFilter();
}

void FilteredTagTableWidget::insertRow(int rowNumber, int lineNumber, QString tagName, QString textInsideTag)
{
    insertText2Cell(rowNumber, COLUMN_INDEX_LINE_NUMBER, QString::number(lineNumber));
    insertText2Cell(rowNumber, COLUMN_INDEX_TAG_NAME, tagName);
    insertText2Cell(rowNumber, COLUMN_INDEX_CONTENT, textInsideTag);
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
}

void FilteredTagTableWidget::insertRow(int row, const TaggedLine& entry)
{
    int line = std::get<COLUMN_INDEX_LINE_NUMBER>(entry);
    const QString& tag = std::get<COLUMN_INDEX_TAG_NAME>(entry);
    const QString& text = std::get<COLUMN_INDEX_CONTENT>(entry);

    auto* itemLine = new QTableWidgetItem(QString::number(line));
    auto* itemTag = new QTableWidgetItem(tag);
    auto* itemText = new QTableWidgetItem(text);
    itemText->setToolTip(text);

    itemTag->setData(Qt::UserRole, tag);

    setItem(row, COLUMN_INDEX_LINE_NUMBER, itemLine);
    setItem(row, COLUMN_INDEX_TAG_NAME, itemTag);
    setItem(row, COLUMN_INDEX_CONTENT, itemText);
}

void FilteredTagTableWidget::updateFilterMenu()
{
    tagFilterMenu->clear();

    QSet<QString> allTags;
    const QModelIndexList matches = model()->match(model()->index(0, COLUMN_INDEX_TAG_NAME), Qt::DisplayRole, QVariant(), -1);

    for (const QModelIndex& idx : matches)
        allTags.insert(idx.data().toString());

    for (const QString& tag : allTags)
    {
        QAction* action = new QAction(tag, this);
        action->setCheckable(true);
        action->setChecked(visibleTags.contains(tag));

        connect(action, &QAction::toggled, this, [this, tag](bool checked) {
            if (checked)
                visibleTags.insert(tag);
            else
                visibleTags.remove(tag);
            applyTagFilter();
        });

        tagFilterMenu->addAction(action);
    }

    if (!allTags.isEmpty()) {
        tagFilterMenu->addSeparator();
        QAction* clearAction = new QAction(tr("Wyczyść filtr"), this);
        connect(clearAction, &QAction::triggered, this, [this]() {
            visibleTags.clear();
            applyTagFilter();
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
