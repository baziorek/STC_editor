#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>
#include "PastedTextDiffDialog.h"
#include "DiffViewerWidget.h"
#include "CodeEditor.h"
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

// --- line-level edits of a QTextDocument (block == line); callers group them in an edit block ---

void replaceLine(QTextDocument *doc, int index, const QString &text)
{
    const QTextBlock block = doc->findBlockByNumber(index);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.insertText(text); // an empty text just removes the selection
}

void removeLine(QTextDocument *doc, int index)
{
    const QTextBlock block = doc->findBlockByNumber(index);
    if (!block.isValid())
        return;

    QTextCursor cursor(doc);
    const int blockTextEnd = block.position() + block.length() - 1;

    if (block.previous().isValid())
    {
        // the separator before this line goes away together with its text
        cursor.setPosition(block.previous().position() + block.previous().length() - 1);
        cursor.setPosition(blockTextEnd, QTextCursor::KeepAnchor);
    }
    else if (block.next().isValid())
    {
        cursor.setPosition(block.position());
        cursor.setPosition(block.next().position(), QTextCursor::KeepAnchor);
    }
    else // the only line
    {
        cursor.setPosition(block.position());
        cursor.setPosition(blockTextEnd, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
}

/// afterIndex == -1 inserts before the first line
void insertLineAfter(QTextDocument *doc, int afterIndex, const QString &text)
{
    QTextCursor cursor(doc);

    if (afterIndex < 0)
    {
        cursor.setPosition(0);
        cursor.insertText(text + '\n');
        return;
    }

    const QTextBlock block = doc->findBlockByNumber(std::min(afterIndex, doc->blockCount() - 1));
    cursor.setPosition(block.position() + block.length() - 1);
    cursor.insertText('\n' + text);
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

    auto *hintLabel = new QLabel(tr("← apply this change to the editor    → discard this change (keep the editor's line)"), diffContainer);
    hintLabel->setStyleSheet("color: gray");
    diffLayout->addWidget(hintLabel);

    diffWidget = new DiffViewerWidget(diffContainer);
    diffWidget->setRowActions(DiffViewerWidget::RowActions::AcceptOrDiscardChange);
    diffWidget->setHorizontalHeaderLabels({tr("Editor #"), tr("Editor line"), tr("Pasted #"), tr("Pasted line"), QString()});
    diffWidget->horizontalHeaderItem(0)->setToolTip(tr("Line number in the editor"));
    diffWidget->horizontalHeaderItem(1)->setToolTip(tr("Line content in the editor (current)"));
    diffWidget->horizontalHeaderItem(2)->setToolTip(tr("Line number in the pasted text"));
    diffWidget->horizontalHeaderItem(3)->setToolTip(tr("Line content in the pasted text"));
    connect(diffWidget, &DiffViewerWidget::changeAccepted, this, &PastedTextDiffDialog::acceptChange);
    connect(diffWidget, &DiffViewerWidget::changeDiscarded, this, &PastedTextDiffDialog::discardChange);
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
    return matchTrailingNewline(pasteEdit->toPlainText(), editor->toPlainText());
}

void PastedTextDiffDialog::pasteFromClipboard()
{
    pasteEdit->setPlainText(QGuiApplication::clipboard()->text());
}

void PastedTextDiffDialog::recomputeDiff()
{
    recomputeTimer.stop();

    const int keptScroll = diffWidget->verticalScrollBar()->value();

    if (pasteEdit->toPlainText().isEmpty())
    {
        fullDiff.clear();
        diffWidget->setDiffData({});
        statsLabel->setText(tr("Paste the new version of the text above."));
        applyButton->setEnabled(false);
        return;
    }

    const QStringList oldLines = editor->toPlainText().split('\n');
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

    diffWidget->setDiffData(diffs);
    diffWidget->verticalScrollBar()->setValue(keptScroll); // stay where the user was after accepting/discarding a change

    if (diffs.isEmpty())
        statsLabel->setText(tr("No differences - the pasted text is identical to the editor content."));
    else
        statsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                .arg(modified).arg(added).arg(removed));

    applyButton->setEnabled(!diffs.isEmpty());
}

const DiffCalculation::DiffLine *PastedTextDiffDialog::findFullDiffLine(int oldIndex, int newIndex, int *position) const
{
    for (size_t i = 0; i < fullDiff.size(); ++i)
    {
        if (fullDiff[i].oldIndex == oldIndex && fullDiff[i].newIndex == newIndex)
        {
            if (position)
                *position = static_cast<int>(i);
            return &fullDiff[i];
        }
    }
    return nullptr;
}

void PastedTextDiffDialog::acceptChange(int row)
{
    if (row < 0 || row >= diffWidget->diffData().size())
        return;

    const auto &result = diffWidget->diffData()[row];
    int position = -1;
    const auto *line = findFullDiffLine(result.oldLineIndex, result.newLineIndex, &position);
    if (!line)
        return;

    // The editor is what changes: one edit block == one undo step
    QTextDocument *doc = editor->document();
    QTextCursor group(doc);
    group.beginEditBlock();

    if (line->oldIndex >= 0 && line->newIndex >= 0)      // modified: take the pasted version of the line
    {
        replaceLine(doc, line->oldIndex, line->newText);
    }
    else if (line->oldIndex >= 0)                        // the pasted text has no such line: remove it from the editor
    {
        removeLine(doc, line->oldIndex);
    }
    else                                                 // the pasted text has an extra line: insert it after its editor neighbour
    {
        int insertAfter = -1;
        for (int i = position - 1; i >= 0; --i)
        {
            if (fullDiff[i].oldIndex >= 0)
            {
                insertAfter = fullDiff[i].oldIndex;
                break;
            }
        }
        insertLineAfter(doc, insertAfter, line->newText);
    }

    group.endEditBlock();

    // Deferred: the clicked button belongs to the table that is about to be rebuilt
    QTimer::singleShot(0, this, &PastedTextDiffDialog::recomputeDiff);
}

void PastedTextDiffDialog::discardChange(int row)
{
    if (row < 0 || row >= diffWidget->diffData().size())
        return;

    const auto &result = diffWidget->diffData()[row];
    int position = -1;
    const auto *line = findFullDiffLine(result.oldLineIndex, result.newLineIndex, &position);
    if (!line)
        return;

    // The pasted text is what changes (its own undo stack keeps working)
    QTextDocument *doc = pasteEdit->document();
    QTextCursor group(doc);
    group.beginEditBlock();

    if (line->oldIndex >= 0 && line->newIndex >= 0)      // modified: put the editor's line back into the pasted text
    {
        replaceLine(doc, line->newIndex, line->oldText);
    }
    else if (line->newIndex >= 0)                        // extra line in the pasted text: drop it
    {
        removeLine(doc, line->newIndex);
    }
    else                                                 // line missing in the pasted text: bring the editor's line back
    {
        int insertAfter = -1;
        for (int i = position - 1; i >= 0; --i)
        {
            if (fullDiff[i].newIndex >= 0)
            {
                insertAfter = fullDiff[i].newIndex;
                break;
            }
        }
        insertLineAfter(doc, insertAfter, line->oldText);
    }

    group.endEditBlock();

    QTimer::singleShot(0, this, &PastedTextDiffDialog::recomputeDiff);
}

void PastedTextDiffDialog::applyPastedText()
{
    const QString oldText = editor->toPlainText();
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
