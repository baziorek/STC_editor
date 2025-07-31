#include <QFileInfo>
#include <QDir>
#include "RenameFileDialog.h"
#include "ui_RenameFileDialog.h"


RenameFileDialog::RenameFileDialog(const QString &oldFileName, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RenameFileDialog)
{
    ui->setupUi(this);

    QFileInfo fileInfo(oldFileName);
    ui->oldFilePathLabel->setText(fileInfo.absoluteFilePath());
    ui->filePathLineEdit->setText(fileInfo.dir().absolutePath());
    ui->fileNameLineEdit->setText(fileInfo.fileName());
}

RenameFileDialog::~RenameFileDialog()
{
    delete ui;
}

QString RenameFileDialog::newFilePath() const
{
    return ui->filePathLineEdit->text();
}

QString RenameFileDialog::newFileName() const
{
    return ui->fileNameLineEdit->text();
}

QString RenameFileDialog::newAbsoluteFilePath() const
{
    QDir dir(ui->filePathLineEdit->text());
    return dir.filePath(ui->fileNameLineEdit->text());
}

bool RenameFileDialog::createDirectoryChecked() const
{
    return ui->createDirCheckBox->isChecked();
}
