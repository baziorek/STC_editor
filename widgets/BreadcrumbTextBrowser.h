#pragma once

#include <QTextBrowser>
#include <QTextCursor>
#include <QPointer>

class CodeEditor;
class FilteredTagTableWidget;

class BreadcrumbTextBrowser : public QTextBrowser
{
    Q_OBJECT

public:
    explicit BreadcrumbTextBrowser(QWidget* parent = nullptr);

    void setTextEditor(CodeEditor* editor);
    void setHeaderTable(FilteredTagTableWidget* table);

    void updateBreadcrumb(const QTextCursor& cursor);

signals:
    void goToLineAndOffsetRequested(int lineNumber, int positionInLine);

protected:
    void extractHeadersBeforePosition(int cursorPos, QMap<int, QPair<QString, int>> &headers, int &outStartScanPos) const;
    QStack<QPair<QString, int>> collectContextTags(const QTextDocument *doc, int startPos, int cursorPos) const;

private slots:
    void onAnchorClicked(const QUrl& link);

private:
    QPointer<CodeEditor> textEditor;
    QPointer<FilteredTagTableWidget> headerTable;

    QString buildBreadcrumbHtml(const QTextCursor& cursor);
    QString tagLink(int pos, const QString& label);
};
