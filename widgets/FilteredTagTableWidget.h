#pragma once

#include <QHash>
#include <QMap>
#include <QTextCursor>
#include <QTreeWidget>
#include <QList>
#include <QObject>
#include <QString>

class QMenu;
class QRegularExpression;
class CodeEditor;
class QWidget;

/// The conspect of the document: its headers ([h1]...[h6]) as a tree. A header is a child of the closest header above
/// it with a smaller level, so [h2] sections sit under their [h1] section, and so on.
/// Tags which are switched off in the filter menu are left out of the tree; the headers under them are moved up.
class FilteredTagTableWidget : public QTreeWidget
{
    Q_OBJECT

public:
    struct HeaderInfo
    {
        QTextCursor startingTagCursor;
        QString tagName;
        QString textInside;
        int startPos;
        int endPos;
    };

    explicit FilteredTagTableWidget(QWidget* parent = nullptr);
    ~FilteredTagTableWidget();

    void setTextEditor(CodeEditor* newTextEditor);

    void rebuildAllHeaders();

    void clear();

    const QList<HeaderInfo>& getCachedHeaders() const
    {
        return cachedHeaders;
    }

signals:
    void goToLineClicked(int lineNumber);

public slots:
    void highlightCurrentTagInContextTable();

private slots:
    void onHeaderSectionClicked(int logicalIndex);
    void onItemClicked(QTreeWidgetItem* item, int column);
    void updateFilterMenu();
    void applyTagFilter();
    void onTextChanged(int pos, int charsRemoved, int charsAdded);

protected:
    static QRegularExpression headerRegex();

    void showEvent(QShowEvent* event) override;

    void refreshHeaderTable();
    void rebuildTree();
    void clearHeaderTable();

    int findHeaderForCursor(const QTextCursor &cursor) const;

    void highlightHeader(int headerIndex);
    void clearHighlightedHeader();

private:
    QMenu* tagFilterMenu = {};
    QMap<QString, bool> tagVisibility;
    CodeEditor* textEditor = {};

    QList<HeaderInfo> cachedHeaders;

    /// the item of every header which is in the tree (the headers switched off are not)
    QHash<int, QTreeWidgetItem*> itemOfHeader;
    int highlightedHeader = -1;
};
