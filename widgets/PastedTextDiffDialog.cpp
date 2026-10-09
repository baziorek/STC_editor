#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QPair>
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

// --- line-level edits of a QTextDocument (block == line); callers group them in an edit block ---

/// Removes `count` lines starting at `first`
void removeLines(QTextDocument *doc, int first, int count)
{
    const int last = std::min(first + count - 1, doc->blockCount() - 1);
    const QTextBlock firstBlock = doc->findBlockByNumber(first);
    const QTextBlock lastBlock = doc->findBlockByNumber(last);
    if (count <= 0 || !firstBlock.isValid() || !lastBlock.isValid())
        return;

    const int lastTextEnd = lastBlock.position() + lastBlock.length() - 1;
    QTextCursor cursor(doc);

    if (firstBlock.previous().isValid())
    {
        // the separator before the first removed line goes away together with the lines
        cursor.setPosition(firstBlock.previous().position() + firstBlock.previous().length() - 1);
        cursor.setPosition(lastTextEnd, QTextCursor::KeepAnchor);
    }
    else if (lastBlock.next().isValid())
    {
        cursor.setPosition(firstBlock.position());
        cursor.setPosition(lastBlock.next().position(), QTextCursor::KeepAnchor);
    }
    else // everything
    {
        cursor.setPosition(firstBlock.position());
        cursor.setPosition(lastTextEnd, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
}

/// afterIndex == -1 inserts before the first line
void insertLinesAfter(QTextDocument *doc, int afterIndex, const QStringList &lines)
{
    if (lines.isEmpty())
        return;

    const QString text = lines.join('\n');
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

/// Replaces `count` lines starting at `first` with `lines`. With count == 0 the lines are inserted after `insertAfter`
/// (-1 == at the beginning); with no `lines` the old ones are just removed.
void replaceLines(QTextDocument *doc, int first, int count, int insertAfter, const QStringList &lines)
{
    if (count <= 0)
    {
        insertLinesAfter(doc, insertAfter, lines);
        return;
    }

    if (lines.isEmpty())
    {
        removeLines(doc, first, count);
        return;
    }

    const QTextBlock firstBlock = doc->findBlockByNumber(first);
    const QTextBlock lastBlock = doc->findBlockByNumber(std::min(first + count - 1, doc->blockCount() - 1));
    if (!firstBlock.isValid() || !lastBlock.isValid())
        return;

    QTextCursor cursor(doc);
    cursor.setPosition(firstBlock.position());
    cursor.setPosition(lastBlock.position() + lastBlock.length() - 1, QTextCursor::KeepAnchor);
    cursor.insertText(lines.join('\n'));
}

void replaceLine(QTextDocument *doc, int index, const QString &text)
{
    replaceLines(doc, index, 1, -1, {text});
}

void removeLine(QTextDocument *doc, int index)
{
    removeLines(doc, index, 1);
}

void insertLineAfter(QTextDocument *doc, int afterIndex, const QString &text)
{
    insertLinesAfter(doc, afterIndex, {text});
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

    auto *hintLabel = new QLabel(tr("← apply to the editor    → discard (keep the editor's line)    - per line, or per block of consecutive changed lines (blue header)"), diffContainer);
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
    connect(diffWidget, &DiffViewerWidget::blockAccepted, this, &PastedTextDiffDialog::acceptBlock);
    connect(diffWidget, &DiffViewerWidget::blockDiscarded, this, &PastedTextDiffDialog::discardBlock);
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

    const int keptScroll = diffWidget->verticalScrollBar()->value();

    if (pasteEdit->toPlainText().isEmpty())
    {
        fullDiff.clear();
        blocks.clear();
        diffWidget->setBlocks({});
        diffWidget->setDiffData({});
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

    diffWidget->setBlocks(findBlocks(diffs));
    diffWidget->setDiffData(diffs);
    diffWidget->verticalScrollBar()->setValue(keptScroll); // stay where the user was after accepting/discarding a change

    if (diffs.isEmpty())
        statsLabel->setText(tr("No differences - the pasted text is identical to the editor content."));
    else
        statsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                .arg(modified).arg(added).arg(removed));

    applyButton->setEnabled(!diffs.isEmpty());
}

/// Runs of two or more consecutive changed lines (no unchanged line in between) become blocks.
QList<DiffViewerWidget::Block> PastedTextDiffDialog::findBlocks(const QList<DiffCalculation::LineDiffResult> &rows)
{
    using DiffCalculation::DiffType;

    blocks.clear();
    QList<DiffViewerWidget::Block> widgetBlocks;

    QHash<QPair<int, int>, int> rowOfChange; // (old index, new index) -> row of `rows`
    for (int row = 0; row < rows.size(); ++row)
        rowOfChange.insert({rows[row].oldLineIndex, rows[row].newLineIndex}, row);

    auto lineRange = [](int first, int count) {
        if (count <= 0)
            return tr("none");
        return count == 1 ? QString::number(first + 1) : QString("%1–%2").arg(first + 1).arg(first + count);
    };

    size_t i = 0;
    while (i < fullDiff.size())
    {
        if (fullDiff[i].type == DiffType::Unchanged)
        {
            ++i;
            continue;
        }

        size_t j = i;
        while (j + 1 < fullDiff.size() && fullDiff[j + 1].type != DiffType::Unchanged)
            ++j;

        if (j > i)
        {
            Block block;
            block.firstEntry = i;
            block.lastEntry = j;

            for (size_t e = i; e <= j; ++e)
            {
                if (fullDiff[e].oldIndex >= 0)
                {
                    if (block.oldFirst < 0)
                        block.oldFirst = fullDiff[e].oldIndex;
                    ++block.oldCount;
                }
                if (fullDiff[e].newIndex >= 0)
                {
                    if (block.newFirst < 0)
                        block.newFirst = fullDiff[e].newIndex;
                    ++block.newCount;
                }
            }
            for (size_t e = i; e-- > 0;)
            {
                if (block.insertAfterOld < 0 && fullDiff[e].oldIndex >= 0)
                    block.insertAfterOld = fullDiff[e].oldIndex;
                if (block.insertAfterNew < 0 && fullDiff[e].newIndex >= 0)
                    block.insertAfterNew = fullDiff[e].newIndex;
                if (block.insertAfterOld >= 0 && block.insertAfterNew >= 0)
                    break;
            }

            const int firstRow = rowOfChange.value({fullDiff[i].oldIndex, fullDiff[i].newIndex}, -1);
            const int lastRow = rowOfChange.value({fullDiff[j].oldIndex, fullDiff[j].newIndex}, -1);

            if (firstRow >= 0 && lastRow - firstRow == static_cast<int>(j - i)) // every changed line has a row
            {
                DiffViewerWidget::Block widgetBlock;
                widgetBlock.firstRow = firstRow;
                widgetBlock.lastRow = lastRow;
                widgetBlock.title = tr("Block of %1 changes  |  editor %2  ↔  pasted %3")
                                        .arg(j - i + 1)
                                        .arg(lineRange(block.oldFirst, block.oldCount))
                                        .arg(lineRange(block.newFirst, block.newCount));
                widgetBlocks.append(widgetBlock);
                blocks.push_back(block);
            }
        }
        i = j + 1;
    }

    return widgetBlocks;
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

void PastedTextDiffDialog::acceptBlock(int blockIndex)
{
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size()))
        return;

    const Block block = blocks[blockIndex];

    QStringList pastedLines;
    for (size_t e = block.firstEntry; e <= block.lastEntry; ++e)
        if (fullDiff[e].newIndex >= 0)
            pastedLines << fullDiff[e].newText;

    // The editor is what changes: one edit block == one undo step for the whole block
    QTextDocument *doc = editor->document();
    QTextCursor group(doc);
    group.beginEditBlock();
    replaceLines(doc, block.oldFirst, block.oldCount, block.insertAfterOld, pastedLines);
    group.endEditBlock();

    QTimer::singleShot(0, this, &PastedTextDiffDialog::recomputeDiff);
}

void PastedTextDiffDialog::discardBlock(int blockIndex)
{
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks.size()))
        return;

    const Block block = blocks[blockIndex];

    QStringList editorLines;
    for (size_t e = block.firstEntry; e <= block.lastEntry; ++e)
        if (fullDiff[e].oldIndex >= 0)
            editorLines << fullDiff[e].oldText;

    // The pasted text is what changes
    QTextDocument *doc = pasteEdit->document();
    QTextCursor group(doc);
    group.beginEditBlock();
    replaceLines(doc, block.newFirst, block.newCount, block.insertAfterNew, editorLines);
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
