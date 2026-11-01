// Which parts of an STC document can be folded: the sections of headers and the blocks of code. Only the algorithm, no widget.

#include <gtest/gtest.h>

#include <QStringList>

#include "utils/FoldRegions.h"

using stcfold::Kind;
using stcfold::Region;
using stcfold::findRegions;

namespace
{
QStringList linesOf(const QString& text)
{
    return text.split('\n');
}
} // namespace

TEST(FoldRegions, HeaderSectionEndsBeforeTheNextHeaderOfTheSameLevel)
{
    const auto regions = findRegions(linesOf(
        "[h1]First[/h1]\n"   // 0
        "text\n"             // 1
        "more text\n"        // 2
        "[h1]Second[/h1]\n"  // 3
        "text"));            // 4

    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ(regions[0].startLine, 0);
    EXPECT_EQ(regions[0].endLine, 2);
    EXPECT_EQ(regions[0].kind, Kind::Header);
    EXPECT_EQ(regions[0].level, 1);
    EXPECT_EQ(regions[0].tag, "h1");
    EXPECT_EQ(regions[1].startLine, 3);
    EXPECT_EQ(regions[1].endLine, 4);
}

TEST(FoldRegions, DeeperHeadersAreInsideTheSectionAndShallowerOnesEndIt)
{
    const auto regions = findRegions(linesOf(
        "[h1]A[/h1]\n"   // 0
        "[h2]B[/h2]\n"   // 1
        "b text\n"       // 2
        "[h3]C[/h3]\n"   // 3
        "c text\n"       // 4
        "[h2]D[/h2]\n"   // 5
        "d text\n"       // 6
        "[h1]E[/h1]\n"   // 7
        "e text"));      // 8

    ASSERT_EQ(regions.size(), 5);
    EXPECT_EQ((std::pair{regions[0].startLine, regions[0].endLine}), (std::pair{0, 6})); // A holds B, C and D
    EXPECT_EQ((std::pair{regions[1].startLine, regions[1].endLine}), (std::pair{1, 4})); // B holds C, ends before D
    EXPECT_EQ((std::pair{regions[2].startLine, regions[2].endLine}), (std::pair{3, 4})); // C
    EXPECT_EQ((std::pair{regions[3].startLine, regions[3].endLine}), (std::pair{5, 6})); // D
    EXPECT_EQ((std::pair{regions[4].startLine, regions[4].endLine}), (std::pair{7, 8})); // E
}

TEST(FoldRegions, EmptyLinesBeforeTheNextSectionStayVisible)
{
    const auto regions = findRegions(linesOf(
        "[h1]A[/h1]\n"   // 0
        "text\n"         // 1
        "\n"             // 2
        "   \n"          // 3
        "[h1]B[/h1]\n"   // 4
        "text"));        // 5

    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ(regions[0].endLine, 1);
}

TEST(FoldRegions, HeaderWithNothingUnderItCannotBeFolded)
{
    EXPECT_TRUE(findRegions(linesOf("[h1]A[/h1]\n[h1]B[/h1]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("[h1]A[/h1]\n[h1]B[/h1]\n\n")).isEmpty()); // only empty lines under B
    EXPECT_TRUE(findRegions(linesOf("[h1]Only[/h1]")).isEmpty());
    EXPECT_TRUE(findRegions({}).isEmpty());
}

TEST(FoldRegions, BlockOfCodeKeepsItsClosingTagVisible)
{
    const auto regions = findRegions(linesOf(
        "text\n"            // 0
        "[cpp]\n"           // 1
        "int main()\n"      // 2
        "{\n"               // 3
        "}\n"               // 4
        "[/cpp]\n"          // 5
        "text"));           // 6

    ASSERT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].kind, Kind::Code);
    EXPECT_EQ(regions[0].tag, "cpp");
    EXPECT_EQ(regions[0].level, 0);
    EXPECT_EQ(regions[0].startLine, 1);
    EXPECT_EQ(regions[0].endLine, 4);        // the lines 2..4 are hidden
    EXPECT_EQ(regions[0].extentEndLine, 5);  // [/cpp] belongs to the block for the user
    EXPECT_EQ(regions[0].hiddenLinesCount(), 3);
    EXPECT_TRUE(regions[0].hides(2));
    EXPECT_TRUE(regions[0].hides(4));
    EXPECT_FALSE(regions[0].hides(1));
    EXPECT_FALSE(regions[0].hides(5));
    EXPECT_TRUE(regions[0].encloses(5));
}

TEST(FoldRegions, CodeBeforeTheClosingTagInItsLineIsHiddenWithTheBlock)
{
    const auto regions = findRegions(linesOf(
        "[cpp]\n"                 // 0
        "int a;\n"                // 1
        "int b;[/cpp]\n"          // 2
        "text"));                 // 3

    ASSERT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].endLine, 2);
    EXPECT_EQ(regions[0].extentEndLine, 2);
}

TEST(FoldRegions, BlockOfCodeInOneLineOrWithNothingInsideIsNotFoldable)
{
    EXPECT_TRUE(findRegions(linesOf("[cpp]int x;[/cpp]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("[cpp]\n[/cpp]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("[cpp]x\n[/cpp]")).isEmpty());  // the code is in the line of the opening tag: nothing to hide
    EXPECT_EQ(findRegions(linesOf("[cpp]\nx\n[/cpp]")).size(), 1);   // one line of code is enough
}

TEST(FoldRegions, AllKindsOfCodeTagsAndTheirAttributesAreRecognized)
{
    const auto regions = findRegions(linesOf(
        "[code]\na\n[/code]\n"                  // 0..2
        "[code src=\"c++\"]\nb\n[/code]\n"      // 3..5
        "[py]\nc\n[/py]\n"                      // 6..8
        "[log]\nd\n[/log]\n"                    // 9..11
        "[CPP]\ne\n[/CPP]"));                   // 12..14

    ASSERT_EQ(regions.size(), 5);
    EXPECT_EQ(regions[0].tag, "code");
    EXPECT_EQ(regions[1].tag, "code");
    EXPECT_EQ(regions[2].tag, "py");
    EXPECT_EQ(regions[3].tag, "log");
    EXPECT_EQ(regions[4].tag, "cpp"); // case does not matter
    for (const Region& region : regions)
        EXPECT_EQ(region.kind, Kind::Code);
}

TEST(FoldRegions, ContentOfCodeIsVerbatimSoHeadersAndTagsInsideStartNothing)
{
    const auto regions = findRegions(linesOf(
        "[h1]A[/h1]\n"            // 0
        "[cpp]\n"                 // 1
        "// [h1]not a header[/h1]\n" // 2
        "char c[2]; // [cpp]\n"   // 3
        "[/cpp]\n"                // 4
        "[h1]B[/h1]\n"            // 5
        "text"));                 // 6

    ASSERT_EQ(regions.size(), 3);
    EXPECT_EQ(regions[0].tag, "h1"); // A: 0..4, the "header" inside the program does not end it
    EXPECT_EQ(regions[0].endLine, 4);
    EXPECT_EQ(regions[1].tag, "cpp");
    EXPECT_EQ(regions[1].startLine, 1);
    EXPECT_EQ(regions[2].tag, "h1");
    EXPECT_EQ(regions[2].startLine, 5);
}

TEST(FoldRegions, BlockWithoutTheClosingTagIsNotFoldable)
{
    EXPECT_TRUE(findRegions(linesOf("[cpp]\nint x;\nint y;")).isEmpty());
}

TEST(FoldRegions, EscapedTagsAreText)
{
    EXPECT_TRUE(findRegions(linesOf("\\[cpp]\nx\n[/cpp]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("\\[h1]x\ny\nz")).isEmpty());
}

TEST(FoldRegions, ClosingTagMustBeOfTheSameKind)
{
    const auto regions = findRegions(linesOf(
        "[cpp]\n"       // 0
        "[/code]\n"     // 1: does not close [cpp]
        "x\n"           // 2
        "[/cpp]"));     // 3
    ASSERT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].endLine, 2);
}

TEST(FoldRegions, HeaderAndCodeBlockInTheSameLineHeaderIsFirst)
{
    const auto regions = findRegions(linesOf(
        "[h2]Title[/h2][cpp]\n"
        "x\n"
        "[/cpp]\n"
        "text"));
    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ(regions[0].kind, Kind::Header);
    EXPECT_EQ(regions[1].kind, Kind::Code);
}

TEST(FoldRegions, RegionsAreSortedByTheirFirstLine)
{
    const auto regions = findRegions(linesOf(
        "[h1]A[/h1]\n"      // 0
        "[cpp]\n"           // 1
        "x\n"               // 2
        "[/cpp]\n"          // 3
        "[h2]B[/h2]\n"      // 4
        "t\n"               // 5
        "[py]\n"            // 6
        "y\n"               // 7
        "[/py]"));          // 8
    ASSERT_EQ(regions.size(), 4);
    for (int i = 1; i < regions.size(); ++i)
        EXPECT_LT(regions[i - 1].startLine, regions[i].startLine);
}

TEST(FoldRegions, DivIsFoldableAlsoWithAttributes)
{
    const auto regions = findRegions(linesOf(
        "text\n"                           // 0
        "[div class=\"tip\"]\n"            // 1
        "advice\n"                         // 2
        "more advice\n"                    // 3
        "[/div]\n"                         // 4
        "[div]\n"                          // 5
        "x\n"                              // 6
        "[/div]"));                         // 7

    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ(regions[0].kind, Kind::Div);
    EXPECT_EQ(regions[0].tag, "div");
    EXPECT_EQ(regions[0].startLine, 1);
    EXPECT_EQ(regions[0].endLine, 3);
    EXPECT_EQ(regions[0].extentEndLine, 4);
    EXPECT_EQ(regions[1].startLine, 5);
    EXPECT_EQ(regions[1].endLine, 6);
}

TEST(FoldRegions, DivInOneLineOrWithoutClosingTagIsNotFoldable)
{
    EXPECT_TRUE(findRegions(linesOf("[div]text[/div]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("[div]\n[/div]")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("[div]\ntext\nmore")).isEmpty());
    EXPECT_TRUE(findRegions(linesOf("text\n[/div]\nmore")).isEmpty()); // a closing tag with nothing to close
}

TEST(FoldRegions, NestedDivsPairWithTheNearestClosingTag)
{
    const auto regions = findRegions(linesOf(
        "[div]\n"          // 0
        "a\n"              // 1
        "[div]\n"          // 2
        "b\n"              // 3
        "[/div]\n"         // 4
        "c\n"              // 5
        "[/div]"));         // 6

    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ((std::pair{regions[0].startLine, regions[0].endLine}), (std::pair{0, 5})); // outer
    EXPECT_EQ((std::pair{regions[1].startLine, regions[1].endLine}), (std::pair{2, 3})); // inner
}

TEST(FoldRegions, DivInsideOfCodeIsCode)
{
    const auto regions = findRegions(linesOf(
        "[cpp]\n"
        "// [div]\n"
        "x\n"
        "[/cpp]\n"
        "[/div]"));
    ASSERT_EQ(regions.size(), 1);
    EXPECT_EQ(regions[0].kind, Kind::Code);
}

TEST(FoldRegions, BlocksStartingInTheSameLineBiggerFirst)
{
    const auto regions = findRegions(linesOf(
        "[div][cpp]\n"      // 0
        "x\n"               // 1
        "[/cpp]\n"          // 2
        "y\n"               // 3
        "[/div]"));          // 4
    ASSERT_EQ(regions.size(), 2);
    EXPECT_EQ(regions[0].kind, Kind::Div);
    EXPECT_EQ(regions[1].kind, Kind::Code);
}

TEST(FoldRegions, TagNamesOnlyLikeHeadersAreHeaders)
{
    // `[h7]`, `[h]`, `[hr]` and `[h1x]` are not headers
    EXPECT_TRUE(findRegions(linesOf("[h7]a[/h7]\nx\n[h]b\ny\n[hr]\nz\n[h1x]\nw")).isEmpty());
    EXPECT_EQ(findRegions(linesOf("[h1 class=\"x\"]a[/h1]\ntext")).size(), 1);
}
