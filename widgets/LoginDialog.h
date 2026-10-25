#pragma once

#include <QDialog>
#include <QObject>
#include <QString>

class QLineEdit;
class QCheckBox;
class QWidget;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString username() const;
    QString password() const;
    bool isRememberChecked() const;
    bool isAutoLoginChecked() const;
    void setCredentials(const QString &username, const QString &password);
    void setRememberChecked(bool remember);
    void setAutoLoginChecked(bool autoLogin);

private slots:
    void onRememberToggled(bool checked);

private:
    QLineEdit *userEdit;
    QLineEdit *passEdit;
    QCheckBox *rememberCheck;
    QCheckBox *autoLoginCheck;
};
