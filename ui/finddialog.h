#pragma once

#include <QWidget>

namespace Ui {
class FindDialog;
}

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

public slots:
    void currentTextChanged(QString newText);

private:
    Ui::FindDialog *ui;
};
