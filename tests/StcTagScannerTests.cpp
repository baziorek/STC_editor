// Reading the STC tags, pairing them, and finding the tag which matches the selected one. Only the text, no widget.

#include <gtest/gtest.h>

#include "utils/StcTagScanner.h"

using StcTagScanner::Range;
using StcTagScanner::matchingTag;
using StcTagScanner::parseTagAt;
using StcTagScanner::scan;

namespace
{
/// The range of the `which`-th occurrence of `what` in `text`
Range rangeOf(const QString& text, const QString& what, int which = 0)
{
    int from = 0;
    int found = -1;
    for (int i = 0; i <= which; ++i)
    {
        found = static_cast<int>(text.indexOf(what, from));
        from = found + 1;
    }
    return {found, found + static_cast<int>(what.size())};
}
} // namespace

TEST(StcTagScanner, ParsesOpeningTagWithAttributes)
{
    const QString text = "[a href=\"https://x.y\" name=\"some text\"]";
    const auto token = parseTagAt(text, 0);
    ASSERT_TRUE(token.has_value());
    EXPECT_FALSE(token->closing);
    EXPECT_EQ(token->name, "a");
    EXPECT_EQ(token->range, (Range{0, static_cast<int>(text.size())}));
    EXPECT_EQ(token->nameRange, (Range{1, 2}));
    ASSERT_EQ(token->attributeValues.size(), 2);
    EXPECT_EQ(text.mid(token->attributeValues[1].start, token->attributeValues[1].length()), "some text");
}

TEST(StcTagScanner, ParsesClosingTagAndItsName)
{
    const QString text = "x[/ Cytat]";
    const auto token = parseTagAt(text, 1);
    ASSERT_TRUE(token.has_value());
    EXPECT_TRUE(token->closing);
    EXPECT_EQ(token->name, "cytat");
    EXPECT_EQ(text.mid(token->nameRange.start, token->nameRange.length()), "Cytat"); // as it is written
}

TEST(StcTagScanner, SomethingInBracketsIsNotATag)
{
    for (const QString& text : {"[0]", "[i++]", "[]", "[=x]", "[unfinished", "[a\nb]", "[a [b]"})
        EXPECT_FALSE(parseTagAt(text, 0).has_value()) << text.toStdString();
}

TEST(StcTagScanner, VerbatimTags)
{
    for (const char* name : {"cpp", "code", "py", "log"})
        EXPECT_TRUE(StcTagScanner::isVerbatimTag(name));
    for (const char* name : {"div", "b", "csv", "h1"})
        EXPECT_FALSE(StcTagScanner::isVerbatimTag(name));
    EXPECT_EQ(StcTagScanner::verbatimTagsAlternation(), "cpp|code|py|log");
}

TEST(StcTagScanner, ContainsVerbatimTagFindsOpeningAndClosingOnes)
{
    EXPECT_TRUE(StcTagScanner::containsVerbatimTag("text [cpp]"));
    EXPECT_TRUE(StcTagScanner::containsVerbatimTag("[code src=\"c++\"]"));
    EXPECT_TRUE(StcTagScanner::containsVerbatimTag("x[/py]"));
    EXPECT_FALSE(StcTagScanner::containsVerbatimTag("a[i] = [b]x[/b]"));
    EXPECT_FALSE(StcTagScanner::containsVerbatimTag(""));
}

TEST(StcTagScanner, PairsNestedTagsAndLeavesTagsWithoutClosingOnesUnpaired)
{
    const QString text = "[div]a [b]bold[/b] [a href=\"x\" name=\"y\"] [img src=\"z\"] b[/div]";
    const auto found = scan(text);

    ASSERT_EQ(found.pairs.size(), 2); // b and div
    EXPECT_EQ(found.openingTags.size(), 4);
    for (const auto& pair : found.pairs)
    {
        // `[img src=...]` and `[a href=...]` are not in a pair, not even when they are between the tags of a pair
        EXPECT_NE(text.mid(pair.openName.start, pair.openName.length()), "img");
        EXPECT_NE(text.mid(pair.openName.start, pair.openName.length()), "a");
    }
}

TEST(StcTagScanner, VerbatimContentIsNotScanned)
{
    const QString text = "[cpp]int a[2]; // [b] [/b]\n[/cpp] [b]x[/b]";
    const auto found = scan(text);
    ASSERT_EQ(found.pairs.size(), 2);
    EXPECT_EQ(found.openingTags.size(), 2); // [cpp] and [b] after it, not the [b] in the program
}

TEST(StcTagScanner, MatchingTagForOpeningName)
{
    const QString text = "[h1]Title[/h1]";
    const auto other = matchingTag(text, rangeOf(text, "h1", 0));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, rangeOf(text, "h1", 1));
}

TEST(StcTagScanner, MatchingTagForClosingNameGivesTheOpeningOne)
{
    const QString text = "[h1]Title[/h1]";
    const auto other = matchingTag(text, rangeOf(text, "h1", 1));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, rangeOf(text, "h1", 0));
}

TEST(StcTagScanner, MatchingTagWithAttributesInTheOpeningTag)
{
    const QString text = "[div class=\"tip\"]x[/div]";
    const auto other = matchingTag(text, rangeOf(text, "div", 0));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, rangeOf(text, "div", 1));

    // the attribute is not the name: nothing matches
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "class")).has_value());
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "tip")).has_value());
}

TEST(StcTagScanner, MatchingTagForTheWholeTag)
{
    const QString text = "[b]x[/b]";
    EXPECT_EQ(matchingTag(text, rangeOf(text, "[b]")), rangeOf(text, "[/b]"));
    EXPECT_EQ(matchingTag(text, rangeOf(text, "[/b]")), rangeOf(text, "[b]"));
}

TEST(StcTagScanner, TagsWithoutClosingTagHaveNoMatch)
{
    const QString text = "[a href=\"x\" name=\"y\"] and [img src=\"z\"] and [b]x[/b]";
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "a")).has_value());
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "img")).has_value());
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "[img src=\"z\"]")).has_value());
    EXPECT_TRUE(matchingTag(text, rangeOf(text, "b", 1)).has_value()); // the `b` in [b]
}

TEST(StcTagScanner, UnclosedTagDoesNotStealTheClosingTagOfTheOuterOne)
{
    // [a href] has no closing tag: [/b] belongs to [b]
    const QString text = "[b]x [a href=\"u\" name=\"n\"] y[/b]";
    const auto other = matchingTag(text, rangeOf(text, "b", 0));
    ASSERT_TRUE(other.has_value());
    const int closing = static_cast<int>(text.indexOf("[/b]"));
    EXPECT_EQ(*other, (Range{closing + 2, closing + 3}));
}

TEST(StcTagScanner, SameNamesNestedPairWithTheNearest)
{
    const QString text = "[div]a[div]b[/div]c[/div]";
    // the inner opening tag goes with the first closing one
    const Range innerOpen{static_cast<int>(text.indexOf("div", 6)), static_cast<int>(text.indexOf("div", 6)) + 3};
    const auto other = matchingTag(text, innerOpen);
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, rangeOf(text, "div", 2));
    // the outer one with the last
    EXPECT_EQ(matchingTag(text, rangeOf(text, "div", 0)), rangeOf(text, "div", 3));
}

TEST(StcTagScanner, NamesInVerbatimContentOrTextAreNotTags)
{
    const QString text = "[cpp]x[i] = 1;[/cpp] i";
    // `i` in the program
    EXPECT_FALSE(matchingTag(text, rangeOf(text, "i", 0)).has_value());
    // an empty selection and a word of the text
    EXPECT_FALSE(matchingTag(text, {3, 3}).has_value());
    EXPECT_FALSE(matchingTag("some words [b]x[/b]", rangeOf("some words [b]x[/b]", "words")).has_value());
}

TEST(StcTagScanner, VerbatimTagsAreMatchedToo)
{
    const QString text = "[cpp]\nint x;\n[/cpp]";
    const auto other = matchingTag(text, rangeOf(text, "cpp", 0));
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, rangeOf(text, "cpp", 1));
    EXPECT_EQ(matchingTag(text, *other), rangeOf(text, "cpp", 0));
}
