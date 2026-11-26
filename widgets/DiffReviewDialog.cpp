#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>
#include <QFont>
#include <QDateTime>
#include <QTextCursor>
#include <QTextDocument>
#include <Qt>
#include <algorithm>

#include "DiffReviewDialog.h"
#include "DiffLineEdit.h"
#include "CodeEditor.h"
#include "utils/DiffCalculation.h"
#include "utils/DiffCalculator.h"

class QWidget;


DiffReviewDialog::DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, QWidget* parent)
    : QDialog(parent), dialogMode(UnsavedChanges), editor(editor)
{
    setupDialogTitleAndLayout(dialogTitle);

    if (!dialogMessage.isEmpty())
        addDialogMessage(dialogMessage);

    const QString filePath = editor->getFileName();
    if (filePath.isEmpty())
    {
        addFileNotSavedMessage();
    }
    else
    {
        setupFileInfoHeader(filePath);
        setupDiffArea(editor);
        setupEditorConnections(editor);
    }

    setupButtons();
}

DiffReviewDialog::DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, 
                                   const QStringList& newFileContent, DialogMode mode, QWidget* parent)
    : QDialog(parent), dialogMode(mode), externalNewContent(newFileContent), editor(editor)
{
    setupDialogTitleAndLayout(dialogTitle);

    if (!dialogMessage.isEmpty())
        addDialogMessage(dialogMessage);

    const QString filePath = editor->getFileName();
    if (filePath.isEmpty())
    {
        addFileNotSavedMessage();
    }
    else
    {
        setupFileInfoHeader(filePath);
        setupDiffAreaForExternalChange(editor);
        setupEditorConnections(editor);
    }

    setupButtonsForExternalChange();
}

void DiffReviewDialog::setupDialogTitleAndLayout(const QString& dialogTitle)
{
    setWindowTitle(dialogTitle.isEmpty()
                   ? tr("Unsaved Changes Detected")
                   : dialogTitle);
    resize(1000, 700);
    mainLayout = new QVBoxLayout(this);
}

void DiffReviewDialog::addDialogMessage(const QString& message)
{
    QLabel* msgLabel = new QLabel(message, this);
    msgLabel->setWordWrap(true);
    QFont font = msgLabel->font();
    font.setPointSizeF(font.pointSizeF() + 1);
    msgLabel->setFont(font);
    mainLayout->addWidget(msgLabel);
}

void DiffReviewDialog::addFileNotSavedMessage()
{
    QLabel* infoLabel = new QLabel(tr("This file has not been saved yet — no diff available."), this);
    QFont font = infoLabel->font();
    font.setItalic(true);
    font.setPointSizeF(font.pointSizeF() + 1);
    infoLabel->setFont(font);
    infoLabel->setStyleSheet("color: gray");
    infoLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(infoLabel, 1);
}

void DiffReviewDialog::setupFileInfoHeader(const QString& filePath)
{
    QFileInfo fi(filePath);

    fileLabel = new QLabel(fi.fileName(), this);
    QFont f = font();
    f.setPointSize(f.pointSize() + 2);
    f.setBold(true);
    fileLabel->setFont(f);

    filePathLabel = new QLabel(fi.absolutePath(), this);
    QFont smallFont = font();
    smallFont.setPointSizeF(font().pointSizeF() - 1);
    filePathLabel->setFont(smallFont);
    filePathLabel->setStyleSheet("color: gray");

    modifiedStatsLabel = new QLabel(this);
    timestampLabel = new QLabel(this);
    timestampLabel->setStyleSheet("color: gray");

    auto* headerLayout = new QVBoxLayout();
    headerLayout->addWidget(fileLabel);
    headerLayout->addWidget(filePathLabel);
    headerLayout->addWidget(modifiedStatsLabel);
    headerLayout->addWidget(timestampLabel);
    mainLayout->addLayout(headerLayout);
}

void DiffReviewDialog::setupDiffArea(CodeEditor* editor)
{
    diffWidget = new SideBySideDiffWidget(this);
    diffWidget->setActions(SideBySideDiffWidget::Actions::RestoreOnly);
    diffWidget->setTitles(tr("Saved version"), tr("Current version"));
    mainLayout->addWidget(diffWidget, 1); // 1 means Expanding

    refreshDiff(editor);
}

void DiffReviewDialog::setupDiffAreaForExternalChange(CodeEditor* editor)
{
    diffWidget = new SideBySideDiffWidget(this);
    diffWidget->setActions(SideBySideDiffWidget::Actions::RestoreOnly);
    diffWidget->setTitles(tr("Editor"), tr("File on disk"));
    mainLayout->addWidget(diffWidget, 1); // 1 means Expanding

    refreshDiff(editor);
}

void DiffReviewDialog::setupEditorConnections(CodeEditor* editor)
{
    connect(diffWidget, &SideBySideDiffWidget::jumpToLineInEditor, editor, &CodeEditor::go2LineRequested);

    connect(diffWidget, &SideBySideDiffWidget::regionDiscarded, this, [this](int regionIndex) {
        restoreRegion(regionIndex);
    });
}

void DiffReviewDialog::refreshDiff(CodeEditor* editor)
{
    pendingScroll_ = diffWidget ? diffWidget->scrollValue() : 0;

    QStringList oldLines, newLines;
    if (dialogMode == ExternalFileChange)
    {
        oldLines = editor->exactLines();
        newLines = externalNewContent;
    }
    else
    {
        oldLines = editor->getOriginalLines();
        newLines = editor->exactLines();
    }

    // plain text appears right away; the aligned, colored diff replaces it when ready
    diffWidget->setPlainLines(oldLines, newLines);
    modifiedStatsLabel->setText(tr("Computing diff…"));

    if (!calculator)
    {
        calculator = new DiffCalculator(this);
        connect(calculator, &DiffCalculator::twoWayReady, this,
                [this](quint64 generation, std::vector<DiffCalculation::DiffLine> diff,
                       QList<DiffCalculation::LineDiffResult> lineDiffs) {
            if (generation != pendingGeneration_)
                return; // a newer recompute was requested meanwhile

            fullDiff = std::move(diff);
            diffWidget->setDiffData(fullDiff, lineDiffs);
            diffWidget->setScrollValue(pendingScroll_);
            populateMetadata(fullDiff);
        });
    }

    pendingGeneration_ = calculator->computeTwoWay(oldLines, newLines);
}

/// "↩" on a run of changes puts the old side's lines back into the editor.
/// Which side of the diff the editor document is depends on the mode: for unsaved changes
/// the editor holds the "new" side, for an external change it holds the "old" side.
void DiffReviewDialog::restoreRegion(int regionIndex)
{
    if (!diffWidget || regionIndex < 0 || regionIndex >= diffWidget->changeRegions().size())
        return;

    const auto &region = diffWidget->changeRegions()[regionIndex];
    const bool editorIsNewSide = (dialogMode == UnsavedChanges);

    const auto oldSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, true);
    const auto newSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, false);
    const auto &editorSide = editorIsNewSide ? newSide : oldSide;

    QTextDocument *doc = editor->document();
    QTextCursor group(doc);
    group.beginEditBlock();
    DiffLineEdit::replaceLines(doc, editorSide.first, editorSide.count, editorSide.insertAfter, oldSide.texts);
    group.endEditBlock();

    refreshDiff(editor);
}

void DiffReviewDialog::setupButtons()
{
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* saveButton = new QPushButton(tr("Save"), this);
    QPushButton* discardButton = new QPushButton(tr("Discard Changes"), this);
    QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

    connect(saveButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Save;
        accept();
    });
    connect(discardButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Discard;
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Cancel;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(discardButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

void DiffReviewDialog::setupButtonsForExternalChange()
{
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* reloadButton = new QPushButton(tr("Reload"), this);
    QPushButton* discardButton = new QPushButton(tr("Discard External Changes"), this);
    QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

    connect(reloadButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Reload;
        accept();
    });
    connect(discardButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Discard;
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Cancel;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addWidget(discardButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

DiffReviewDialog::Result DiffReviewDialog::userChoice() const
{
    return selectedResult;
}

void DiffReviewDialog::populateMetadata(const std::vector<DiffCalculation::DiffLine>& diffLines)
{
    int added = 0, removed = 0, modified = 0;

    for (const auto& line : diffLines)
    {
        switch (line.type)
        {
        case DiffCalculation::DiffType::Added: ++added; break;
        case DiffCalculation::DiffType::Removed: ++removed; break;
        case DiffCalculation::DiffType::Modified: ++modified; break;
        case DiffCalculation::DiffType::Unchanged: break;
        }
    }

    modifiedStatsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                    .arg(modified)
                                    .arg(added)
                                    .arg(removed));

    const QDateTime fileTime = editor ? editor->getFileModificationTime() : QDateTime();
    const QDateTime lastEditTime = editor ? editor->getLastChangeTime() : QDateTime();
    QString timestampText = tr("File last modified: %1 | Last edit in session: %2")
                                .arg(fileTime.toString("yyyy-MM-dd hh:mm:ss"))
                                .arg(lastEditTime.toString("yyyy-MM-dd hh:mm:ss"));

    timestampLabel->setText(timestampText);
}
