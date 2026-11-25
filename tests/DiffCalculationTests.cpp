#include <gtest/gtest.h>
#include "utils/DiffCalculation.h"

using DiffCalculation::DiffLine;
using DiffCalculation::DiffType;

namespace
{
/// The texts of one side of the diff, in the order in which they stand in the document
QStringList sideTexts(const std::vector<DiffLine>& diff, bool newSide)
{
    QStringList texts;
    for (const DiffLine& line : diff)
    {
        const int index = newSide ? line.newIndex : line.oldIndex;
        if (index < 0)
            continue;
        texts << (newSide ? line.newText : line.oldText);
    }
    return texts;
}

/// The diff must describe exactly the two input documents: every old line appears once on the
/// old side (in order), every new line once on the new side (in order).
void expectReconstructs(const QStringList& oldLines, const QStringList& newLines,
                        const std::vector<DiffLine>& diff)
{
    EXPECT_EQ(sideTexts(diff, false), oldLines);
    EXPECT_EQ(sideTexts(diff, true), newLines);
}

QList<DiffType> typesOf(const std::vector<DiffLine>& diff)
{
    QList<DiffType> types;
    for (const DiffLine& line : diff)
        types << line.type;
    return types;
}
} // namespace

// ------------------------------------------------------------------ computeDiff: whole lines

TEST(DiffCalculationLines, EmptyDocuments)
{
    EXPECT_TRUE(DiffCalculation::computeDiff({}, {}).empty());

    const auto added = DiffCalculation::computeDiff({}, {QStringLiteral("x")});
    ASSERT_EQ(added.size(), 1);
    EXPECT_EQ(added[0].type, DiffType::Added);
    EXPECT_EQ(added[0].oldIndex, -1);
    EXPECT_EQ(added[0].newIndex, 0);

    const auto removed = DiffCalculation::computeDiff({QStringLiteral("x")}, {});
    ASSERT_EQ(removed.size(), 1);
    EXPECT_EQ(removed[0].type, DiffType::Removed);
    EXPECT_EQ(removed[0].oldIndex, 0);
    EXPECT_EQ(removed[0].newIndex, -1);
}

TEST(DiffCalculationLines, IdenticalDocumentsAreAllUnchanged)
{
    const QStringList lines{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const auto diff = DiffCalculation::computeDiff(lines, lines);
    ASSERT_EQ(diff.size(), 3);
    for (int k = 0; k < 3; ++k)
    {
        EXPECT_EQ(diff[k].type, DiffType::Unchanged);
        EXPECT_EQ(diff[k].oldIndex, k);
        EXPECT_EQ(diff[k].newIndex, k);
    }
    expectReconstructs(lines, lines, diff);
}

TEST(DiffCalculationLines, LineInsertedInTheMiddle)
{
    const QStringList oldLines{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const QStringList newLines{QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("b"), QStringLiteral("c")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    ASSERT_EQ(typesOf(diff), (QList<DiffType>{DiffType::Unchanged, DiffType::Added,
                                              DiffType::Unchanged, DiffType::Unchanged}));
    EXPECT_EQ(diff[1].newText, QStringLiteral("x"));
    EXPECT_EQ(diff[2].oldIndex, 1); // "b" is still paired with "b", not shifted onto "x"
    expectReconstructs(oldLines, newLines, diff);
}

TEST(DiffCalculationLines, LineRemovedFromTheMiddle)
{
    const QStringList oldLines{QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("b"), QStringLiteral("c")};
    const QStringList newLines{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    ASSERT_EQ(typesOf(diff), (QList<DiffType>{DiffType::Unchanged, DiffType::Removed,
                                              DiffType::Unchanged, DiffType::Unchanged}));
    EXPECT_EQ(diff[1].oldText, QStringLiteral("x"));
    EXPECT_EQ(diff[2].oldIndex, 2);
    expectReconstructs(oldLines, newLines, diff);
}

TEST(DiffCalculationLines, ChangedLineIsPairedAsModified)
{
    const QStringList oldLines{QStringLiteral("int a = 1;"), QStringLiteral("int b = 2;")};
    const QStringList newLines{QStringLiteral("int a = 1;"), QStringLiteral("int b = 3;")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    ASSERT_EQ(diff.size(), 2);
    EXPECT_EQ(diff[0].type, DiffType::Unchanged);
    EXPECT_EQ(diff[1].type, DiffType::Modified);
    EXPECT_EQ(diff[1].oldText, QStringLiteral("int b = 2;"));
    EXPECT_EQ(diff[1].newText, QStringLiteral("int b = 3;"));
    expectReconstructs(oldLines, newLines, diff);
}

TEST(DiffCalculationLines, InsertionInsideChangedBlockDoesNotShiftThePairing)
{
    // "foo bar" becomes "foo bar!" (a replace block) and "new line" is inserted inside the
    // same block: the similar pair must stay together instead of pairing by position.
    const QStringList oldLines{QStringLiteral("x1"), QStringLiteral("x2"), QStringLiteral("foo bar")};
    const QStringList newLines{QStringLiteral("x1"), QStringLiteral("x2"), QStringLiteral("new line"),
                               QStringLiteral("foo bar!")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    ASSERT_EQ(typesOf(diff), (QList<DiffType>{DiffType::Unchanged, DiffType::Unchanged, DiffType::Added,
                                              DiffType::Modified}));
    EXPECT_EQ(diff[2].newText, QStringLiteral("new line"));
    EXPECT_EQ(diff[3].oldText, QStringLiteral("foo bar"));
    EXPECT_EQ(diff[3].newText, QStringLiteral("foo bar!"));
    expectReconstructs(oldLines, newLines, diff);
}

TEST(DiffCalculationLines, RepeatedLines)
{
    const QStringList oldLines{QStringLiteral("x"), QStringLiteral("x"), QStringLiteral("x")};
    const QStringList newLines{QStringLiteral("x"), QStringLiteral("x")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);

    ASSERT_EQ(diff.size(), 3);
    EXPECT_EQ(typesOf(diff).count(DiffType::Unchanged), 2);
    EXPECT_EQ(typesOf(diff).count(DiffType::Removed), 1);
    expectReconstructs(oldLines, newLines, diff);
}

TEST(DiffCalculationLines, WholeDocumentReplaced)
{
    const QStringList oldLines{QStringLiteral("aaa"), QStringLiteral("bbb")};
    const QStringList newLines{QStringLiteral("xxx"), QStringLiteral("yyy"), QStringLiteral("zzz")};
    const auto diff = DiffCalculation::computeDiff(oldLines, newLines);
    expectReconstructs(oldLines, newLines, diff);
}

// ------------------------------------------------------------------ calculateModifiedLines

TEST(DiffCalculationModifiedLines, MarksNewLinesThatAreNotEqual)
{
    const QStringList oldLines{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    const QStringList newLines{QStringLiteral("a"), QStringLiteral("X"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("Y")};

    const QSet<int> modified = DiffCalculation::calculateModifiedLines(oldLines, newLines);
    // line numbers count from 1; the shifted "b" and "c" are equal, only the inserts count
    EXPECT_EQ(modified, (QSet<int>{2, 5}));
}

TEST(DiffCalculationModifiedLines, ChangedLineIsMarked)
{
    const QStringList oldLines{QStringLiteral("a"), QStringLiteral("b")};
    const QStringList newLines{QStringLiteral("a"), QStringLiteral("B")};
    EXPECT_EQ(DiffCalculation::calculateModifiedLines(oldLines, newLines), (QSet<int>{2}));
}

TEST(DiffCalculationModifiedLines, IdenticalDocumentsHaveNoModifiedLines)
{
    const QStringList lines{QStringLiteral("a"), QStringLiteral("b")};
    EXPECT_TRUE(DiffCalculation::calculateModifiedLines(lines, lines).isEmpty());
}

TEST(DiffCalculationModifiedLines, PureDeletionMarksNothingOnTheNewSide)
{
    const QStringList oldLines{QStringLiteral("a"), QStringLiteral("x"), QStringLiteral("b")};
    const QStringList newLines{QStringLiteral("a"), QStringLiteral("b")};
    EXPECT_TRUE(DiffCalculation::calculateModifiedLines(oldLines, newLines).isEmpty());
}
