#pragma once

#include <QWidget>

namespace Ui {
class FindDialog;
}

class FindDialog : public QWidget
{
    Q_OBJECT

public:
    explicit FindDialog(QWidget *parent = nullptr);
    ~FindDialog();

private:
    Ui::FindDialog *ui;
};
