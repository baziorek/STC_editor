#pragma once

#include <QDialog>
#include <QObject>
#include <QString>

class QLabel;
class QVBoxLayout;
class CodeEditor;
class DiffViewerWidget;
class QWidget;

class BackupRecoveryDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result
    {
        RestoreBackup,
        DiscardBackup,
        Cancel
    };

    explicit BackupRecoveryDialog(CodeEditor* editor, const QString& originalFilePath, 
                                  const QString& backupContent, QWidget* parent = nullptr);

    Result userChoice() const;

protected:
    void setupDialog();
    void setupFileInfoHeader(const QString& originalFilePath);
    void setupDiffArea(CodeEditor* editor, const QString& backupContent);
    void setupButtons();

private:
    Result selectedResult = Cancel;
    QString m_originalFilePath;
    QString m_backupContent;

    QVBoxLayout* mainLayout = {};
    QLabel* fileLabel = {};
    QLabel* filePathLabel = {};
    QLabel* timestampLabel = {};
    DiffViewerWidget* diffWidget = {};
};