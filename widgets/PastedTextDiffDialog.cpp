#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QStringList>
#include <QWidget>

#include "PastedTextDiffDialog.h"
#include "DiffLineEdit.h"
#include "CodeEditor.h"
#include "utils/ExactText.h"
#include "utils/DiffCalculation.h"


namespace
{
/// Pasted text often differs from the editor content only by the final newline - it would show up
/// as a bogus "added/removed empty line" in the diff, so the pasted text follows the editor's ending.
QString matchTrailingNewline(QString pasted, const QString &reference)
{
    const bool referenceEndsWithNewline = reference.endsWith('\n');

    if (!referenceEndsWithNewline && pasted.endsWith('\n'))
        pasted.chop(1);
    else if (referenceEndsWithNewline && !pasted.isEmpty() && !pasted.endsWith('\n'))
        pasted += '\n';

    return pasted;
}
} // namespace


PastedTextDiffDialog::PastedTextDiffDialog(CodeEditor *editor, QWidget *parent)
    : QDialog(parent), editor(editor)
{
    setWindowTitle(tr("Compare with pasted text"));
    resize(1100, 750);

    auto *mainLayout = new QVBoxLayout(this);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(splitter, 1);

    // --- top: paste area ---
    auto *pasteContainer = new QWidget(splitter);
    auto *pasteLayout = new QVBoxLayout(pasteContainer);
    pasteLayout->setContentsMargins(0, 0, 0, 0);

    auto *pasteHeader = new QHBoxLayout();
    pasteHeader->addWidget(new QLabel(tr("Paste the new version of the whole text here:"), pasteContainer));
    pasteHeader->addStretch();
    auto *pasteFromClipboardButton = new QPushButton(tr("Paste from clipboard"), pasteContainer);
    pasteHeader->addWidget(pasteFromClipboardButton);
    pasteLayout->addLayout(pasteHeader);

    pasteEdit = new QPlainTextEdit(pasteContainer);
    pasteEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    pasteEdit->setPlaceholderText(tr("Ctrl+V - the diff against the current editor content appears below"));
    pasteLayout->addWidget(pasteEdit, 1);

    // --- bottom: diff ---
    auto *diffContainer = new QWidget(splitter);
    auto *diffLayout = new QVBoxLayout(diffContainer);
    diffLayout->setContentsMargins(0, 0, 0, 0);

    statsLabel = new QLabel(diffContainer);
    diffLayout->addWidget(statsLabel);

    auto *hintLabel = new QLabel(tr("← keep the editor's lines    → take the pasted lines - for each run of consecutive changes"), diffContainer);
    hintLabel->setStyleSheet("color: gray");
    diffLayout->addWidget(hintLabel);

    diffWidget = new SideBySideDiffWidget(diffContainer);
    diffWidget->setActions(SideBySideDiffWidget::Actions::AcceptDiscard);
    diffWidget->setTitles(tr("Editor"), tr("Pasted text"));
    // "←" keeps the editor's (left) version -> the pasted text is edited; "→" takes the pasted (right) version -> the editor is edited
    connect(diffWidget, &SideBySideDiffWidget::regionDiscarded, this, &PastedTextDiffDialog::discardRegion);
    connect(diffWidget, &SideBySideDiffWidget::regionAccepted, this, &PastedTextDiffDialog::acceptRegion);
    connect(diffWidget, &SideBySideDiffWidget::jumpToLineInEditor, editor, &CodeEditor::go2LineRequested);
    diffLayout->addWidget(diffWidget, 1);

    splitter->addWidget(pasteContainer);
    splitter->addWidget(diffContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    // --- buttons ---
    auto *buttonLayout = new QHBoxLayout();
    applyButton = new QPushButton(tr("Apply pasted text to editor"), this);
    applyButton->setToolTip(tr("Replaces the whole editor content with the pasted text (can be undone with Ctrl+Z)"));
    auto *closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    recomputeTimer.setSingleShot(true);
    recomputeTimer.setInterval(300);

    connect(&recomputeTimer, &QTimer::timeout, this, &PastedTextDiffDialog::recomputeDiff);
    connect(pasteEdit, &QPlainTextEdit::textChanged, &recomputeTimer, qOverload<>(&QTimer::start));
    connect(pasteFromClipboardButton, &QPushButton::clicked, this, &PastedTextDiffDialog::pasteFromClipboard);
    connect(applyButton, &QPushButton::clicked, this, &PastedTextDiffDialog::applyPastedText);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    // Usually the new text has just been copied, so use it right away
    const QString clipboardText = QGuiApplication::clipboard()->text();
    if (!clipboardText.isEmpty())
        pasteEdit->setPlainText(clipboardText);

    recomputeDiff();
    pasteEdit->setFocus();
}

QString PastedTextDiffDialog::pastedTextMatchingEditorEnding() const
{
    return matchTrailingNewline(exactPlainText(pasteEdit->document()), editor->exactText());
}

void PastedTextDiffDialog::pasteFromClipboard()
{
    pasteEdit->setPlainText(QGuiApplication::clipboard()->text());
}

void PastedTextDiffDialog::recomputeDiff()
{
    recomputeTimer.stop();

    const int keptScroll = diffWidget->scrollValue();

    if (pasteEdit->toPlainText().isEmpty())
    {
        fullDiff.clear();
        diffWidget->setDiffLines({});
        statsLabel->setText(tr("Paste the new version of the text above."));
        applyButton->setEnabled(false);
        return;
    }

    const QStringList oldLines = editor->exactLines();
    const QStringList newLines = pastedTextMatchingEditorEnding().split('\n');

    fullDiff = DiffCalculation::computeDiff(oldLines, newLines);
    const auto diffs = DiffCalculation::computeModifiedLineDiffs(fullDiff);

    int added = 0, removed = 0, modified = 0;
    for (const auto &diff : diffs)
    {
        if (diff.oldLineIndex == -1)
            ++added;
        else if (diff.newLineIndex == -1)
            ++removed;
        else
            ++modified;
    }

    diffWidget->setDiffLines(fullDiff);
    diffWidget->setScrollValue(keptScroll); // stay where the user was after accepting/discarding a change

    if (diffs.isEmpty())
        statsLabel->setText(tr("No differences - the pasted text is identical to the editor content."));
    else
        statsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                .arg(modified).arg(added).arg(removed));

    applyButton->setEnabled(!diffs.isEmpty());
}

/// "→" on a run of changes: the pasted lines replace the editor's lines of that run.
void PastedTextDiffDialog::acceptRegion(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= diffWidget->changeRegions().size())
        return;

    const auto &region = diffWidget->changeRegions()[regionIndex];
    const auto oldSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, true);
    const auto newSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, false);

    // The editor is what changes: one edit block == one undo step
    QTextDocument *doc = editor->document();
    QTextCursor group(doc);
    group.beginEditBlock();
    DiffLineEdit::replaceLines(doc, oldSide.first, oldSide.count, oldSide.insertAfter, newSide.texts);
    group.endEditBlock();

    // Deferred: the clicked button belongs to the widget that is about to be rebuilt
    QTimer::singleShot(0, this, &PastedTextDiffDialog::recomputeDiff);
}

/// "←" on a run of changes: the editor's lines replace the pasted lines of that run,
/// so the difference disappears from the pasted text.
void PastedTextDiffDialog::discardRegion(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= diffWidget->changeRegions().size())
        return;

    const auto &region = diffWidget->changeRegions()[regionIndex];
    const auto oldSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, true);
    const auto newSide = DiffLineEdit::sideLinesOfRegion(fullDiff, region.firstEntry, region.lastEntry, false);

    // The pasted text is what changes (its own undo stack keeps working)
    QTextDocument *doc = pasteEdit->document();
    QTextCursor group(doc);
    group.beginEditBlock();
    DiffLineEdit::replaceLines(doc, newSide.first, newSide.count, newSide.insertAfter, oldSide.texts);
    group.endEditBlock();

    QTimer::singleShot(0, this, &PastedTextDiffDialog::recomputeDiff);
}

void PastedTextDiffDialog::applyPastedText()
{
    const QString oldText = editor->exactText();
    const QString newText = pastedTextMatchingEditorEnding();
    if (newText == oldText)
        return;

    const int cursorBlock = editor->textCursor().blockNumber();
    const int cursorColumn = editor->textCursor().positionInBlock();
    const int scrollValue = editor->verticalScrollBar()->value();

    // One edit block == one undo step
    QTextCursor cursor(editor->document());
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(newText);
    cursor.endEditBlock();

    // Keep the user roughly where they were
    QTextDocument *doc = editor->document();
    const QTextBlock block = doc->findBlockByNumber(std::min(cursorBlock, doc->blockCount() - 1));
    QTextCursor restored(block);
    restored.setPosition(block.position() + std::min(cursorColumn, std::max(0, block.length() - 1)));
    editor->setTextCursor(restored);
    editor->verticalScrollBar()->setValue(scrollValue);

    accept();
}
