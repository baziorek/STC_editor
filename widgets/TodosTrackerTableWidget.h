#pragma once

#include <QTableWidget>
#include <QMap>
#include <QTextCursor>

class CodeEditor;


class TodoTrackerTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    struct TodoInfo
    {
        QTextCursor cursor;   // live position in document
        QString text;         // todo message
    };

    explicit TodoTrackerTableWidget(QWidget* parent = nullptr);

    void setTextEditor(CodeEditor* newEditor);

    CodeEditor* getTextEditor() const { return textEditor; }

signals:
    void goToLineRequested(int lineNumber);
    void goToLineAndOffsetRequested(int lineNumber, int linePosition);
    void todosTotalCountChanged(int totalTodos);

private slots:
    void onCellSingleClicked(int row, int);
    void onLineContentChanged(int position, int, int);

protected:
    void scanEntireDocumentDetectingAllTodos();
    void showEvent(QShowEvent *event) override;

    void refreshTable();
private:
    void setupTable();

private:
    CodeEditor* textEditor = nullptr;
    QList<TodoInfo> todoList;

    static inline const QRegularExpression todoRegex{R"(\bTODO\b[:]? *(.*))", QRegularExpression::CaseInsensitiveOption};
};
