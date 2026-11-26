#include "FileDiffDialog.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

#include "CodeEditor.h"
#include "utils/DiffCalculation.h"
#include "utils/FileEncodingHandler.h"
#include "SideBySideDiffWidget.h"

FileDiffDialog::FileDiffDialog(CodeEditor* editor, const QString& otherFilePath, QWidget* parent)
    : QDialog(parent), editor(editor)
{
    init(editor->exactLines(),
         FileEncodingHandler::readFile(otherFilePath).split('\n'),
         tr("Compare with %1").arg(QFileInfo(otherFilePath).fileName()));
}

FileDiffDialog::FileDiffDialog(const QString& oldFilePath, const QString& newFilePath, QWidget* parent)
    : QDialog(parent)
{
    init(FileEncodingHandler::readFile(oldFilePath).split('\n'),
         FileEncodingHandler::readFile(newFilePath).split('\n'),
         tr("Compare %1 with %2").arg(QFileInfo(oldFilePath).fileName(), QFileInfo(newFilePath).fileName()));
}

void FileDiffDialog::init(const QStringList& oldLines, const QStringList& newLines, const QString& title)
{
    setWindowTitle(title);
    setMinimumSize(900, 600);

    auto* layout = new QVBoxLayout(this);

    statsLabel = new QLabel(this);
    layout->addWidget(statsLabel);

    diffWidget = new SideBySideDiffWidget(this);
    diffWidget->setActions(SideBySideDiffWidget::Actions::None);
    diffWidget->setTitles(tr("Old version"), tr("New version"));
    layout->addWidget(diffWidget, 1);

    if (editor)
        connect(diffWidget, &SideBySideDiffWidget::jumpToLineInEditor,
                editor, &CodeEditor::go2LineRequested);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

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

    diffWidget->setDiffLines(fullDiff);

    if (diffs.isEmpty())
        statsLabel->setText(tr("No differences - the files are identical."));
    else
        statsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                .arg(modified).arg(added).arg(removed));
}
