// Tests of ThreeWayDiff: hunks of two sides are merged into clean changes or conflicts,
// the aligned rows keep the three documents on the same line count, and the merged
// text takes the changed side or marks the conflict.
#include <gtest/gtest.h>

#include "utils/ThreeWayDiff.h"

namespace
{
using ThreeWayDiff::RowKind;

QStringList lines(std::initializer_list<const char *> texts)
{
    QStringList result;
    for (const char *text : texts)
        result << QString::fromUtf8(text);
    return result;
}

int countKind(const ThreeWayDiff::Result &result, RowKind kind)
{
    int count = 0;
    for (const auto &row : result.rows)
        if (row.kind == kind)
            ++count;
    return count;
}
} // namespace


TEST(ThreeWayDiff, IdenticalDocumentsHaveOnlyUnchangedRows)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c"}), lines({"a", "b", "c"}), lines({"a", "b", "c"}));

    ASSERT_EQ(result.rows.size(), 3);
    ASSERT_TRUE(result.regions.isEmpty());
    ASSERT_EQ(countKind(result, RowKind::Unchanged), 3);
    ASSERT_EQ(result.mergedLines, lines({"a", "b", "c"}));
}

TEST(ThreeWayDiff, ChangeOnlyInOursIsACleanChange)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c"}), lines({"a", "B", "c"}), lines({"a", "b", "c"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::ChangedOurs);
    ASSERT_EQ(countKind(result, RowKind::ChangedOurs), 1);
    ASSERT_EQ(result.mergedLines, lines({"a", "B", "c"}));

    // theirs kept the base line in that row
    const auto &row = result.rows[1];
    ASSERT_EQ(row.base, QStringLiteral("b"));
    ASSERT_EQ(row.ours, QStringLiteral("B"));
    ASSERT_EQ(row.theirs, QStringLiteral("b"));
}

TEST(ThreeWayDiff, ChangeOnlyInTheirsIsACleanChange)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c"}), lines({"a", "b", "c"}), lines({"a", "b", "C"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::ChangedTheirs);
    ASSERT_EQ(result.mergedLines, lines({"a", "b", "C"}));
}

TEST(ThreeWayDiff, SameChangeOnBothSidesMergesCleanly)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b"}), lines({"a", "x"}), lines({"a", "x"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::SameChange);
    ASSERT_EQ(result.mergedLines, lines({"a", "x"}));
}

TEST(ThreeWayDiff, DifferentChangesOnTheSameLinesAreAConflict)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c"}), lines({"a", "X", "c"}), lines({"a", "Y", "c"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::Conflict);
    ASSERT_EQ(countKind(result, RowKind::Conflict), 1);

    // the merged text keeps both versions between markers
    ASSERT_EQ(result.mergedLines,
              lines({"a", "<<<<<<< ours", "X", "=======", "Y", ">>>>>>> theirs", "c"}));
}

TEST(ThreeWayDiff, ChangesOnAdjacentLinesAreNotAConflict)
{
    // ours changes line 1, theirs changes line 2 - independent edits next to each other
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c", "d"}),
                                              lines({"a", "B", "c", "d"}),
                                              lines({"a", "b", "C", "d"}));

    ASSERT_EQ(result.regions.size(), 2);
    ASSERT_EQ(result.regions[0].kind, RowKind::ChangedOurs);
    ASSERT_EQ(result.regions[1].kind, RowKind::ChangedTheirs);
    ASSERT_EQ(result.mergedLines, lines({"a", "B", "C", "d"}));
}

TEST(ThreeWayDiff, InsertionOnlyInOurs)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "c"}), lines({"a", "b", "c"}), lines({"a", "c"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::ChangedOurs);

    // the inserted line has no base number; the base pane shows an empty placeholder there
    const auto &row = result.rows[1];
    ASSERT_EQ(row.base, QString());
    ASSERT_EQ(row.baseNumber, -1);
    ASSERT_EQ(row.ours, QStringLiteral("b"));
    ASSERT_EQ(row.theirs, QString());
    ASSERT_EQ(result.mergedLines, lines({"a", "b", "c"}));
}

TEST(ThreeWayDiff, DifferentInsertionsAtTheSamePointAreAConflict)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b"}), lines({"a", "x", "b"}), lines({"a", "y", "b"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::Conflict);
}

TEST(ThreeWayDiff, InsertionAndChangeOfTheSameLineAreAConflict)
{
    // ours inserts before line 1, theirs rewrites line 1
    const auto result = ThreeWayDiff::compute(lines({"a", "b"}), lines({"a", "x", "b"}), lines({"a", "B"}));

    ASSERT_EQ(result.regions.size(), 1);
    ASSERT_EQ(result.regions[0].kind, RowKind::Conflict);
}

TEST(ThreeWayDiff, LineNumbersFollowEachDocument)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c", "d"}),
                                              lines({"a", "x", "y", "c", "d"}),  // two lines inserted at 1
                                              lines({"a", "b", "c", "D"}));     // last line changed

    // unchanged row for base line "c" (index 2): ours index is 3 (x,y inserted before it), theirs 2
    int cRow = -1;
    for (int i = 0; i < result.rows.size(); ++i)
        if (result.rows[i].kind == RowKind::Unchanged && result.rows[i].base == QStringLiteral("c"))
            cRow = i;
    ASSERT_GE(cRow, 0);
    ASSERT_EQ(result.rows[cRow].baseNumber, 2);
    ASSERT_EQ(result.rows[cRow].oursNumber, 3);
    ASSERT_EQ(result.rows[cRow].theirsNumber, 2);

    // the "d" row is a ChangedTheirs region row
    const auto &dRow = result.rows.last();
    ASSERT_EQ(dRow.theirs, QStringLiteral("D"));
    ASSERT_EQ(dRow.theirsNumber, 3);
}

TEST(ThreeWayDiff, AllRowsHaveThreeFieldsAndRegionsAreContiguous)
{
    const auto result = ThreeWayDiff::compute(lines({"a", "b", "c", "d", "e"}),
                                              lines({"a", "X", "c", "d", "e"}),
                                              lines({"a", "b", "c", "Y", "e"}));

    for (const auto &row : result.rows)
    {
        // unchanged rows carry the base text on all sides; region rows carry real or empty lines
        if (row.kind == RowKind::Unchanged)
        {
            ASSERT_EQ(row.base, row.ours);
            ASSERT_EQ(row.base, row.theirs);
            ASSERT_EQ(row.region, -1);
        }
        else
        {
            ASSERT_GE(row.region, 0);
            ASSERT_LT(row.region, result.regions.size());
        }
    }

    for (const auto &region : result.regions)
    {
        ASSERT_GE(region.firstRow, 0);
        ASSERT_LE(region.lastRow, static_cast<int>(result.rows.size()) - 1);
        ASSERT_EQ(result.rows[region.firstRow].kind, region.kind);
    }
}
