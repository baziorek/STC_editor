#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @namespace ThreeWayDiff
 * @brief Three-way (diff3-style) alignment of a base text and two changed versions.
 *
 * computeDiff(base, ours) and computeDiff(base, theirs) are merged into regions:
 * a stretch of the base text that only one side changed is a clean change of that
 * side, a stretch both sides changed differently is a conflict, and a stretch both
 * sides changed in the same way merges cleanly.
 *
 * The result is a list of display rows: every row carries one line of each side
 * (an empty string where the side has no line in that row) and the panes therefore
 * always have the same number of lines.
 */
namespace ThreeWayDiff
{

enum class RowKind
{
    Unchanged,   ///< same on all three sides
    ChangedOurs, ///< only "ours" differs from the base
    ChangedTheirs, ///< only "theirs" differs from the base
    SameChange,  ///< both sides made the same change
    Conflict     ///< both sides changed the same base lines differently
};

struct Row
{
    QString base;
    QString ours;
    QString theirs;
    /// index into the document of that side, or -1 for a placeholder row
    int baseNumber = -1;
    int oursNumber = -1;
    int theirsNumber = -1;
    RowKind kind = RowKind::Unchanged;
    /// index into regions(), or -1 for unchanged rows
    int region = -1;
};

struct Region
{
    int firstRow = 0;
    int lastRow = 0;
    RowKind kind = RowKind::Unchanged;
};

struct Result
{
    QVector<Row> rows;
    QVector<Region> regions;
    /// The automatically merged text; conflicts are marked with <<<<<<< / ======= / >>>>>>> lines.
    QStringList mergedLines;
};

Result compute(const QStringList &base, const QStringList &ours, const QStringList &theirs);

} // namespace ThreeWayDiff
