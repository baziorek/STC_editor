#include <QVBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include "LoginDialog.h"


LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Login to cpp0x.pl");

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Enter your cpp0x.pl login credentials.\n"
                                 "Accounts are free to create."));

    userEdit = new QLineEdit(this);
    userEdit->setPlaceholderText("Username");
    layout->addWidget(userEdit);

    passEdit = new QLineEdit(this);
    passEdit->setPlaceholderText("Password");
    passEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passEdit);

    rememberCheck = new QCheckBox("Remember login", this);
    layout->addWidget(rememberCheck);

    autoLoginCheck = new QCheckBox("Auto login", this);
    autoLoginCheck->setEnabled(false);
    layout->addWidget(autoLoginCheck);

    connect(rememberCheck, &QCheckBox::toggled, this, &LoginDialog::onRememberToggled);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString LoginDialog::username() const
{
    return userEdit->text();
}
QString LoginDialog::password() const
{
    return passEdit->text();
}
bool LoginDialog::isRememberChecked() const
{
    return rememberCheck->isChecked();
}
bool LoginDialog::isAutoLoginChecked() const
{
    return autoLoginCheck->isChecked();
}
void LoginDialog::setCredentials(const QString &username, const QString &password)
{
    userEdit->setText(username);
    passEdit->setText(password);
}
void LoginDialog::setRememberChecked(bool remember)
{
    rememberCheck->setChecked(remember);
}
void LoginDialog::setAutoLoginChecked(bool autoLogin)
{
    autoLoginCheck->setChecked(autoLogin);
}
void LoginDialog::onRememberToggled(bool checked)
{
    autoLoginCheck->setEnabled(checked);
    if (!checked)
    {
        autoLoginCheck->setChecked(false);
    }
}
