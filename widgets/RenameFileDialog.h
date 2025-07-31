#pragma once

#include <QDialog>

namespace Ui {
class RenameFileDialog;
}

class RenameFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RenameFileDialog(const QString& oldFileName, QWidget *parent = nullptr);
    ~RenameFileDialog();

    QString newFilePath() const;
    QString newFileName() const;
    QString newAbsoluteFilePath() const;
    bool createDirectoryChecked() const;

private:
    Ui::RenameFileDialog *ui;
};
