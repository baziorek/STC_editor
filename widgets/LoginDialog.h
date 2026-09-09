#pragma once

#include <QDialog>

class QLineEdit;
class QCheckBox;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;
    bool isRememberChecked() const;
    void setCredentials(const QString &username, const QString &password);
    void setRememberChecked(bool remember);

private:
    QLineEdit *userEdit;
    QLineEdit *passEdit;
    QCheckBox *rememberCheck;
};
