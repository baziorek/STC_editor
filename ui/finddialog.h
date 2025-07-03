#pragma once

#include <QWidget>

namespace Ui {
class FindDialog;
}

class QTreeWidgetItem;

class CodeEditor;

class FindDialog : public QWidget
{
    Q_OBJECT

    CodeEditor* codeEditor{};

public:
    explicit FindDialog(QWidget *parent = nullptr);
    ~FindDialog();

    void setCodeEditor(CodeEditor* codeEditor)
    {
        this->codeEditor = codeEditor;
    }

    void focusInput();

signals:
    void jumpToLocationRequested(int line, int offset);

protected:
    std::pair<int,int> showOccurences(const QString& text);

public slots:
    void currentTextChanged(QString newText);
    void onResultItemClicked(QTreeWidgetItem* item, int column);

    void odCheckboxMatchCasesChanged(bool checked);

private:
    Ui::FindDialog *ui;
};
