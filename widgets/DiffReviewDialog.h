#pragma once

#include <QDialog>
#include "codeeditor.h"
#include "DiffViewerWidget.h"

class QLabel;

class DiffReviewDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result
    {
        Save,
        Discard,
        Cancel
    };

    explicit DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, QWidget* parent = nullptr);

    Result userChoice() const;

protected:
    void populateMetadata(const QStringList& oldLines, const QStringList& newLines, const QString& filePath,
                          const QDateTime& fileTime, const QDateTime& lastEditTime,
                          const QList<DiffCalculation::LineDiffResult>& diffs);

private:
    Result selectedResult = Cancel;

    QLabel* fileLabel;
    QLabel* filePathLabel;
    QLabel* modifiedStatsLabel;
    QLabel* timestampLabel;
    DiffViewerWidget* diffWidget;
};
