#pragma once

#include <QDialog>

namespace Ui {
class RenameFileDialog;
}

class CodeEditor;

class RenameFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RenameFileDialog(CodeEditor *textEditor, QWidget *parent = nullptr);
    ~RenameFileDialog();

    QString newFilePath() const;
    QString newFileName() const;
    QString newAbsoluteFilePath() const;

public slots:
    void validateAndUpdateUi();
    void onBrowseDirectoryClicked();

signals:
    void renameCompleted(const QString& newPath);

protected:
    void tryRenameFile();
    QString getOldAbsoluteFilePath() const;

private:
    Ui::RenameFileDialog *ui;
    CodeEditor *textEditor;
};
