#ifndef ERRORLIST_H
#define ERRORLIST_H

#include <QWidget>

namespace Ui {
class ErrorList;
}

class ErrorList : public QWidget
{
    Q_OBJECT

public:
    explicit ErrorList(QWidget *parent = nullptr);
    ~ErrorList();

    void addError(int errorNumber, QString errorText);
    void clearErrors();

private:
    Ui::ErrorList *ui;
};

#endif // ERRORLIST_H
