#pragma once

#include <QDialog>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <vector>

#include "utils/DiffCalculation.h"
#include "SideBySideDiffWidget.h"

class QLabel;
class QVBoxLayout;
class CodeEditor;
class QDateTime;
class QWidget;

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
    void setupDiffAreaForExternalChange(CodeEditor* editor);
    void setupEditorConnections(CodeEditor* editor);
    void setupButtons();
    void setupButtonsForExternalChange();
    /// Recomputes the diff of the two sides after a restore and refreshes the view and the metadata.
    void refreshDiff(CodeEditor* editor);
    /// The "↩" button of a run of changes: puts the old side's lines back into the editor.
    void restoreRegion(int regionIndex);

private:
    Result selectedResult = Cancel;
    DialogMode dialogMode = UnsavedChanges;
    QStringList externalNewContent;
    CodeEditor* editor = nullptr;

    QVBoxLayout* mainLayout = {};
    QLabel* fileLabel = {};
    QLabel* filePathLabel = {};
    QLabel* modifiedStatsLabel = {};
    QLabel* timestampLabel = {};
    SideBySideDiffWidget* diffWidget = {};
    std::vector<DiffCalculation::DiffLine> fullDiff;
};
