#pragma once

#include <QTableWidget>
#include <QMap>
#include <QTextCursor>

class QMenu;
class QRegularExpression;

class CodeEditor;

class FilteredTagTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit FilteredTagTableWidget(QWidget* parent = nullptr);
    ~FilteredTagTableWidget();

    void setTextEditor(CodeEditor* newTextEditor);

    void rebuildAllHeaders();

    void clear();

signals:
    void goToLineClicked(int lineNumber);

public slots:
    void highlightCurrentTagInContextTable();

private slots:
    void onHeaderSectionClicked(int logicalIndex);
    void onCellSingleClicked(int row, int column);
    void updateFilterMenu();
    void applyTagFilter();
    void onTextChanged(int pos, int charsRemoved, int charsAdded);

protected:
    struct HeaderInfo;

    static QRegularExpression headerRegex();

    void showEvent(QShowEvent* event) override;

    void refreshHeaderTable();

    void insertOrUpdateHeader(const HeaderInfo& info);
    void clearHeaderTable();

    int findHeaderForCursor(const QTextCursor &cursor) const;

    void highlightRow(int row);
    void clearHighlightedRow();

private:
    QMenu* tagFilterMenu = {};
    QMap<QString, bool> tagVisibility;
    CodeEditor* textEditor = {};

    QList<HeaderInfo> cachedHeaders;

    int highlightedRow = -1;
};
