#pragma once

#include <QSet>
#include <QList>
#include <QStringList>

// namespace DiffCalculation // problems with linking for Windows
// {
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
// } // namespace DiffCalculation
