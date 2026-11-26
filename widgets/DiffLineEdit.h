#pragma once

#include <QString>
#include <QStringList>
#include <algorithm>

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

#include "utils/DiffCalculation.h"

/// Line-level edits of a QTextDocument (block == line), shared by the diff dialogs.
/// Callers group them in an edit block, so one user action stays one undo step.
namespace DiffLineEdit
{

/// Removes `count` lines starting at `first`
inline void removeLines(QTextDocument *doc, int first, int count)
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
inline void insertLinesAfter(QTextDocument *doc, int afterIndex, const QStringList &lines)
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
inline void replaceLines(QTextDocument *doc, int first, int count, int insertAfter, const QStringList &lines)
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

inline void replaceLine(QTextDocument *doc, int index, const QString &text)
{
    replaceLines(doc, index, 1, -1, {text});
}

inline void removeLine(QTextDocument *doc, int index)
{
    removeLines(doc, index, 1);
}

inline void insertLineAfter(QTextDocument *doc, int afterIndex, const QString &text)
{
    insertLinesAfter(doc, afterIndex, {text});
}

/// The lines one side of a change region covers in its document: where they start, how many
/// there are, where lines are inserted when there are none (`insertAfter`, -1 == at the
/// beginning) and their texts in order. `oldSide` picks the old (left) or the new (right) side.
struct SideLines
{
    int first = -1;
    int count = 0;
    int insertAfter = -1;
    QStringList texts;
};

/// `firstEntry`/`lastEntry` are indices into the aligned diff (see DiffCalculation::computeDiff).
inline SideLines sideLinesOfRegion(const std::vector<DiffCalculation::DiffLine> &diff,
                                   int firstEntry, int lastEntry, bool oldSide)
{
    SideLines result;

    for (int e = firstEntry; e <= lastEntry && e < static_cast<int>(diff.size()); ++e)
    {
        const auto &line = diff[e];
        const int index = oldSide ? line.oldIndex : line.newIndex;
        if (index < 0)
            continue;

        if (result.first < 0)
            result.first = index;
        ++result.count;
        result.texts << (oldSide ? line.oldText : line.newText);
    }

    // the line of this side right before the region: where new lines go when the region
    // has no lines of this side at all
    for (int e = firstEntry - 1; e >= 0; --e)
    {
        const int index = oldSide ? diff[e].oldIndex : diff[e].newIndex;
        if (index >= 0)
        {
            result.insertAfter = index;
            break;
        }
    }

    return result;
}

} // namespace DiffLineEdit
