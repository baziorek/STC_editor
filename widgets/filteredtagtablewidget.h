#pragma once

#include <QTableWidget>
#include <QSet>
#include <QMenu>
#include <QString>
#include <QHeaderView>

class FilteredTagTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    using TaggedLine = std::tuple<int, QString, QString>; // lineNumber, tag, text
    using TaggedData = QList<TaggedLine>;

    explicit FilteredTagTableWidget(QWidget* parent = nullptr);

    void setData(const TaggedData& data);

    void insertRow(int row, int lineNumber, QString tagNumber, QString textInsideTag);
    void insertText2Cell(int row, int column, const QString& text);

signals:
    void goToLineClicked(int lineNumber);

private slots:
    void updateFilterMenu();
    void applyTagFilter();
    void onCellSingleClicked(int row, int /*column*/);

private:
    QMenu* tagFilterMenu;
    QSet<QString> visibleTags;
    TaggedData allData;

    void insertRow(int row, const TaggedLine& entry);
};
