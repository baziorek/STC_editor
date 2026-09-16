#pragma once

#include <QDialog>
#include "DiffViewerWidget.h"

class QLabel;
class QVBoxLayout;

class CodeEditor;

class DiffReviewDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result
    {
        Save,
        Discard,
        Cancel,
        Reload
    };

    enum DialogMode
    {
        UnsavedChanges,
        ExternalFileChange
    };

    explicit DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, QWidget* parent = nullptr);
    explicit DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, 
                             const QStringList& newFileContent, DialogMode mode, QWidget* parent = nullptr);

    Result userChoice() const;

protected:
    void populateMetadata(const QStringList& oldLines, const QStringList& newLines, const QString& filePath,
                          const QDateTime& fileTime, const QDateTime& lastEditTime,
                          const QList<DiffCalculation::LineDiffResult>& diffs);

    void setupDialogTitleAndLayout(const QString& dialogTitle);
    void addDialogMessage(const QString& message);
    void addFileNotSavedMessage();
    void setupFileInfoHeader(const QString& filePath);
    void setupDiffArea(CodeEditor* editor);
    void setupDiffAreaForExternalChange(CodeEditor* editor, const QStringList& newFileContent);
    void setupEditorConnections(CodeEditor* editor);
    void setupButtons();
    void setupButtonsForExternalChange();
    void handleLineRestoration(CodeEditor* editor, int lineIndex, const QString& restoredText);

private:
    Result selectedResult = Cancel;
    DialogMode dialogMode = UnsavedChanges;
    QStringList externalNewContent;

    QVBoxLayout* mainLayout = {};
    QLabel* fileLabel = {};
    QLabel* filePathLabel = {};
    QLabel* modifiedStatsLabel = {};
    QLabel* timestampLabel = {};
    DiffViewerWidget* diffWidget = {};
};
