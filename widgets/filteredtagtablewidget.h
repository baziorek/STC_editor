#pragma once

#include <QTableWidget>
#include <QMenu>
#include <QString>
#include <QHeaderView>
#include <QMap>

class CodeEditor;

class FilteredTagTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit FilteredTagTableWidget(QWidget* parent = nullptr);

    void insertRow(int row, int lineNumber, QString tagName, QString textInsideTag);
    void insertText2Cell(int row, int column, const QString& text);

    void clearTags();

    CodeEditor *getTextEditor() const
    {
        return textEditor;
    }
    void setTextEditor(CodeEditor *newTextEditor)
    {
        textEditor = newTextEditor;
    }

signals:
    void goToLineClicked(int lineNumber);

public slots:
    void onUpdateContextRequested();

private slots:
    void updateFilterMenu();
    void applyTagFilter();
    void onCellSingleClicked(int row, int /*column*/);
    void onHeaderSectionClicked(int logicalIndex);

protected:
    void highlightCurrentTagInContextTable();

    void showEvent(QShowEvent* event) override;

private:
    QMenu* tagFilterMenu;
    QMap<QString, bool> tagVisibility;

    CodeEditor* textEditor{};
};
