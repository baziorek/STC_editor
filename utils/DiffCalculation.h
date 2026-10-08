#pragma once

#include <QSet>
#include <QList>
#include <QStringList>

namespace DiffCalculation // problems with linking for Windows
{
enum class DiffType
{
    Unchanged,
    Added,
    Removed,
    Modified
};

struct DiffLine
{
    int oldIndex = -1; // line number in oldLines (or -1 if added)
    int newIndex = -1; // line number in newLines (or -1 if removed)
    QString oldText;
    QString newText;
    DiffType type;
};


QSet<int> calculateModifiedLines(const QStringList& oldLines, const QStringList& newLines);

std::vector<DiffLine> computeDiff(const QStringList &oldLines, const QStringList &newLines);


enum class FragmentType
{
    Equal,
    Insert,
    Delete
};

struct LineDiffFragment
{
    FragmentType type;
    QString text;
};

struct LineDiffResult
{
    int oldLineIndex;
    int newLineIndex;
    QList<LineDiffFragment> oldFragments;
    QList<LineDiffFragment> newFragments;

    QString oldText() const
    {
        QString result;
        for (const auto& frag : oldFragments)
            result += frag.text;
        return result;
    }
};

QList<LineDiffResult> computeModifiedLineDiffs(const std::vector<DiffLine>& modifiedLines);
QList<LineDiffResult> computeAllLineDiffs(const std::vector<DiffLine>& diffLines);


struct CharChangeCounts
{
    int inserted = 0;
    int removed = 0;
    bool approximate = false; ///< the texts were too long to be compared character by character
};

/// How many characters (Unicode code points) have to be inserted and removed to turn `oldText` into `newText`.
/// The text which is the same at the start and at the end is skipped first, so a typical edit costs almost nothing.
CharChangeCounts countCharChanges(const QString& oldText, const QString& newText);

/// Character diff of two texts as ONE sequence in text order: equal text, deleted text, inserted text.
/// (computeAllLineDiffs() gives the old and the new side separately - this one is for showing a change inline.)
QList<LineDiffFragment> computeInlineDiff(const QString& oldText, const QString& newText);
} // namespace DiffCalculation
