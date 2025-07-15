#pragma once

#include <QTableWidget>
#include <QMap>

class CodeEditor;


class TodoTrackerTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit TodoTrackerTableWidget(QWidget* parent = nullptr);

    void setTextEditor(CodeEditor* newEditor);

    CodeEditor* getTextEditor() const { return textEditor; }

    void clearTodos();

private slots:
    void onLineContentChanged(int position, int, int);

protected:
    void scanEntireDocumentDetectingAllTodos();
    void showEvent(QShowEvent *event) override;

private:
    void setupTable();

    void updateOrRemoveTodoForLine(int lineNumber, const QString& lineText);

    void removeTodoRow(int lineNumber);

private:
    CodeEditor* textEditor = nullptr;
    QMap<int, int> lineToRowMap; // maps line number to row in table
    // static inline const QRegularExpression todoRegex{R"__(\bTODO\s*:\s*(.*))__"};
    static inline const QRegularExpression todoRegex{R"(\bTODO\b[:]? *(.*))", QRegularExpression::CaseInsensitiveOption};
};
