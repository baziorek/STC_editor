#include "FileDiffDialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

#include "CodeEditor.h"
#include "utils/DiffCalculation.h"
#include "utils/FileEncodingHandler.h"
#include "DiffViewerWidget.h"

FileDiffDialog::FileDiffDialog(CodeEditor* editor, const QString& otherFilePath, QWidget* parent)
    : QDialog(parent)
    , editor(editor)
    , otherFilePath(otherFilePath)
{
    setWindowTitle(tr("Compare with %1").arg(QFileInfo(otherFilePath).fileName()));
    setMinimumSize(900, 600);

    auto* layout = new QVBoxLayout(this);

    statsLabel = new QLabel(this);
    layout->addWidget(statsLabel);

    diffWidget = new DiffViewerWidget(this);
    diffWidget->setRowActions(DiffViewerWidget::RowActions::None);
    layout->addWidget(diffWidget, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    const QString otherText = FileEncodingHandler::readFile(otherFilePath);
    const QStringList oldLines = editor->exactLines();
    const QStringList newLines = otherText.split('\n');

    const auto fullDiff = DiffCalculation::computeDiff(oldLines, newLines);
    const auto diffs = DiffCalculation::computeModifiedLineDiffs(fullDiff);

    int added = 0, removed = 0, modified = 0;
    for (const auto& diff : diffs)
    {
        if (diff.oldLineIndex == -1)
            ++added;
        else if (diff.newLineIndex == -1)
            ++removed;
        else
            ++modified;
    }

    diffWidget->setDiffData(diffs);

    if (diffs.isEmpty())
        statsLabel->setText(tr("No differences - the files are identical."));
    else
        statsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                .arg(modified).arg(added).arg(removed));
}
