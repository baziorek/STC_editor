#pragma once

#include <QDialog>

class QLineEdit;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;

private:
    QLineEdit *userEdit;
    QLineEdit *passEdit;
};
