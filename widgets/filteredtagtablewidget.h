#pragma once

#include <QTableWidget>
#include <QMenu>
#include <QString>
#include <QHeaderView>
#include <QMap>


class FilteredTagTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit FilteredTagTableWidget(QWidget* parent = nullptr);

    void insertRow(int row, int lineNumber, QString tagName, QString textInsideTag);
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
    QMap<QString, bool> tagVisibility;
};
