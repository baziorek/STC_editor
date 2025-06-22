#pragma once

#include <QGroupBox>

namespace Ui
{
    class GoToLineWidget;
}

class GoToLineWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit GoToLineWidget(QWidget *parent = nullptr);
    ~GoToLineWidget();

signals:
    void onGoToLineRequested(int lineNumber);

public slots:
    void setMaxLine(int maxLineNumber);
    void currentLineChanged(int currentLineNumber);
    void onGoToLinePressed();

private:
    Ui::GoToLineWidget *ui;
};
