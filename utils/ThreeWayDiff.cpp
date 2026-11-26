#include "ThreeWayDiff.h"

#include <algorithm>

#include "DiffCalculation.h"

namespace
{

/// One changed stretch of a two-way diff: it replaces `count` base lines starting at
/// `baseStart` (count == 0 means pure insertion before that line) with `lines`.
struct Hunk
{
    int baseStart = 0;
    int baseCount = 0;
    QStringList lines;
};

/// The maximal runs of changed entries of computeDiff() as hunks.
QVector<Hunk> hunksOf(const QStringList &oldLines, const QStringList &newLines)
{
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    QVector<Hunk> hunks;
    int basePos = 0; // base lines consumed so far

    for (size_t i = 0; i < diff.size();)
    {
        if (diff[i].type == DiffCalculation::DiffType::Unchanged)
        {
            basePos = diff[i].oldIndex + 1;
            ++i;
            continue;
        }

        const int baseStart = basePos;
        QStringList lines;
        int baseCount = 0;

        size_t j = i;
        while (j < diff.size() && diff[j].type != DiffCalculation::DiffType::Unchanged)
        {
            if (diff[j].oldIndex >= 0)
                ++baseCount;
            if (diff[j].newIndex >= 0)
                lines << diff[j].newText;
            ++j;
        }

        hunks.append({baseStart, baseCount, lines});
        basePos = baseStart + baseCount;
        i = j;
    }

    return hunks;
}

/// How many lines of the side's document lie before base line `basePos`.
/// Every untouched base line contributes one line; each hunk fully before contributes
/// (its lines - the base lines it replaces). An insertion exactly at `basePos` is NOT
/// before it (its lines come first in the document, but they belong to the row of the
/// change, not to the count of plain lines before the base line).
int sideLinesBefore(const QStringList &base, const QVector<Hunk> &hunks, int basePos)
{
    int count = basePos;
    for (const Hunk &h : hunks)
    {
        if (h.baseStart >= basePos)
            break; // hunks are ordered
        if (h.baseStart + h.baseCount <= basePos)
            count += static_cast<int>(h.lines.size()) - h.baseCount;
    }
    return count;
}

/// The index in the side's document of the (unchanged) base line `baseLine`.
int sideNumberAt(const QStringList &base, const QVector<Hunk> &hunks, int baseLine)
{
    int number = sideLinesBefore(base, hunks, baseLine);
    // insertions exactly at this point come before the base line in the side's document
    for (const Hunk &h : hunks)
    {
        if (h.baseStart > baseLine)
            break;
        if (h.baseStart == baseLine && h.baseCount == 0)
            number += static_cast<int>(h.lines.size());
    }
    return number;
}

/// The lines of the base range [baseStart, baseEnd) after applying the given hunks of one side.
QStringList splicedLines(const QStringList &base, int baseStart, int baseEnd,
                         const QVector<Hunk> &sideHunks)
{
    QStringList result;
    int pos = baseStart;

    for (const Hunk &hunk : sideHunks)
    {
        for (int i = pos; i < hunk.baseStart && i < baseEnd; ++i)
            result << base[i];
        result += hunk.lines;
        pos = std::max(pos, hunk.baseStart + hunk.baseCount);
    }

    for (int i = pos; i < baseEnd; ++i)
        result << base[i];

    return result;
}

/// A stretch of the base text changed by at least one side; the hunks of both sides inside it.
struct Span
{
    int baseStart = 0;
    int baseEnd = 0;
    QVector<int> oursHunks;   // indexes into the ours hunk list
    QVector<int> theirsHunks; // indexes into the theirs hunk list
    ThreeWayDiff::RowKind kind = ThreeWayDiff::RowKind::Unchanged;
    QStringList baseLines, oursLines, theirsLines;
};

} // namespace


ThreeWayDiff::Result ThreeWayDiff::compute(const QStringList &base, const QStringList &ours,
                                           const QStringList &theirs)
{
    Result result;

    const QVector<Hunk> hunksOurs = hunksOf(base, ours);
    const QVector<Hunk> hunksTheirs = hunksOf(base, theirs);

    // --- merge the hunks of both sides into spans ---
    QVector<Span> spans;

    // Hunks overlap when their base ranges intersect; insertions are points, so they join a
    // range when the point lies inside it (inclusive ends), which also merges an insertion
    // with a change of the very same lines into one conflict region.
    const auto overlaps = [](const Hunk &h, int start, int end) {
        const int hEnd = h.baseStart + h.baseCount;
        if (start == end) // the span so far is a single insertion point
            return h.baseStart <= start && start <= hEnd;
        if (h.baseCount == 0)
            return h.baseStart >= start && h.baseStart <= end;
        return h.baseStart < end && start < hEnd;
    };

    size_t i = 0, j = 0;
    while (i < hunksOurs.size() || j < hunksTheirs.size())
    {
        Span span;
        if (j >= hunksTheirs.size() || (i < hunksOurs.size() && hunksOurs[i].baseStart <= hunksTheirs[j].baseStart))
        {
            span.baseStart = hunksOurs[i].baseStart;
            span.baseEnd = hunksOurs[i].baseStart + hunksOurs[i].baseCount;
            span.oursHunks.append(static_cast<int>(i));
            ++i;
        }
        else
        {
            span.baseStart = hunksTheirs[j].baseStart;
            span.baseEnd = hunksTheirs[j].baseStart + hunksTheirs[j].baseCount;
            span.theirsHunks.append(static_cast<int>(j));
            ++j;
        }

        // absorb every hunk of either side that overlaps the growing span
        bool grew = true;
        while (grew)
        {
            grew = false;
            while (i < hunksOurs.size() && overlaps(hunksOurs[i], span.baseStart, span.baseEnd))
            {
                span.oursHunks.append(static_cast<int>(i));
                span.baseStart = std::min(span.baseStart, hunksOurs[i].baseStart);
                span.baseEnd = std::max(span.baseEnd, hunksOurs[i].baseStart + hunksOurs[i].baseCount);
                ++i;
                grew = true;
            }
            while (j < hunksTheirs.size() && overlaps(hunksTheirs[j], span.baseStart, span.baseEnd))
            {
                span.theirsHunks.append(static_cast<int>(j));
                span.baseStart = std::min(span.baseStart, hunksTheirs[j].baseStart);
                span.baseEnd = std::max(span.baseEnd, hunksTheirs[j].baseStart + hunksTheirs[j].baseCount);
                ++j;
                grew = true;
            }
        }

        // the content of the three sides inside the span
        for (int k = span.baseStart; k < span.baseEnd; ++k)
            span.baseLines << base[k];

        QVector<Hunk> oursSide, theirsSide;
        for (int idx : span.oursHunks)
            oursSide << hunksOurs[idx];
        for (int idx : span.theirsHunks)
            theirsSide << hunksTheirs[idx];
        span.oursLines = splicedLines(base, span.baseStart, span.baseEnd, oursSide);
        span.theirsLines = splicedLines(base, span.baseStart, span.baseEnd, theirsSide);

        if (span.oursHunks.isEmpty())
            span.kind = RowKind::ChangedTheirs;
        else if (span.theirsHunks.isEmpty())
            span.kind = RowKind::ChangedOurs;
        else
            span.kind = (span.oursLines == span.theirsLines) ? RowKind::SameChange : RowKind::Conflict;

        spans.append(span);
    }

    // --- emit the rows ---
    int basePos = 0;

    const auto emitUnchanged = [&](int baseLine) {
        Row row;
        row.base = base[baseLine];
        row.ours = row.base;
        row.theirs = row.base;
        row.baseNumber = baseLine;
        row.oursNumber = sideNumberAt(base, hunksOurs, baseLine);
        row.theirsNumber = sideNumberAt(base, hunksTheirs, baseLine);
        row.kind = RowKind::Unchanged;
        result.rows.append(row);
    };

    for (const Span &span : spans)
    {
        while (basePos < span.baseStart)
        {
            emitUnchanged(basePos);
            ++basePos;
        }

        const int firstRow = result.rows.size();
        const int rowCount = std::max({static_cast<int>(span.baseLines.size()),
                                       static_cast<int>(span.oursLines.size()),
                                       static_cast<int>(span.theirsLines.size())});

        const int oursBefore = sideLinesBefore(base, hunksOurs, span.baseStart);
        const int theirsBefore = sideLinesBefore(base, hunksTheirs, span.baseStart);

        for (int k = 0; k < rowCount; ++k)
        {
            Row row;
            row.base = k < span.baseLines.size() ? span.baseLines[k] : QString();
            row.ours = k < span.oursLines.size() ? span.oursLines[k] : QString();
            row.theirs = k < span.theirsLines.size() ? span.theirsLines[k] : QString();
            row.baseNumber = k < span.baseLines.size() ? span.baseStart + k : -1;
            row.oursNumber = k < span.oursLines.size() ? oursBefore + k : -1;
            row.theirsNumber = k < span.theirsLines.size() ? theirsBefore + k : -1;
            row.kind = span.kind;
            row.region = result.regions.size();
            result.rows.append(row);
        }

        result.regions.append({firstRow, static_cast<int>(result.rows.size()) - 1, span.kind});
        basePos = span.baseEnd;
    }

    while (basePos < static_cast<int>(base.size()))
    {
        emitUnchanged(basePos);
        ++basePos;
    }

    // --- the automatically merged text ---
    basePos = 0;
    for (const Span &span : spans)
    {
        while (basePos < span.baseStart)
            result.mergedLines << base[basePos++];

        switch (span.kind)
        {
        case RowKind::ChangedOurs:
        case RowKind::SameChange:
            result.mergedLines += span.oursLines;
            break;
        case RowKind::ChangedTheirs:
            result.mergedLines += span.theirsLines;
            break;
        case RowKind::Conflict:
            result.mergedLines << QStringLiteral("<<<<<<< ours");
            result.mergedLines += span.oursLines;
            result.mergedLines << QStringLiteral("=======");
            result.mergedLines += span.theirsLines;
            result.mergedLines << QStringLiteral(">>>>>>> theirs");
            break;
        case RowKind::Unchanged:
            break;
        }
        basePos = span.baseEnd;
    }
    while (basePos < static_cast<int>(base.size()))
        result.mergedLines << base[basePos++];

    return result;
}
