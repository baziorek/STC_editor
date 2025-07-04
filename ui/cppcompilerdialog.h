#pragma once

#include <QDialog>
#include <QString>

class QTextEdit;
class QPushButton;

class CppCompilerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CppCompilerDialog(const QString& code, QWidget* parent = nullptr);

private:
    void compileCode(const QString& code);
    QTextEdit* outputEdit;
    QPushButton* closeButton;
};
