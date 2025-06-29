#pragma once

#include <QTableWidget>
#include <QSet>
#include <QMenu>
#include <QString>
#include <QHeaderView>
#include <QToolButton>

class FilteredTagTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit FilteredTagTableWidget(QWidget* parent = nullptr);

    void insertRow(int row, int lineNumber, QString tagNumber, QString textInsideTag);
    void insertText2Cell(int row, int column, const QString& text);

signals:
    void goToLineClicked(int lineNumber);

private slots:
    void updateFilterMenu();
    void applyTagFilter();
    void onCellSingleClicked(int row, int /*column*/);
    void onHeaderSectionClicked(int logicalIndex);

private:
    QMenu* tagFilterMenu;
    QSet<QString> visibleTags;
    QSet<QString> allKnownTags;
};
