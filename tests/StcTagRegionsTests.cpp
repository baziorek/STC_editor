#include <gtest/gtest.h>
#include "utils/StcTagRegions.h"

using StcTagRegions::Range;
using StcTagRegions::regionsAround;

namespace
{
/// The position in the middle of `word` (its first occurrence)
int inside(const QString& text, const QString& word)
{
    const int index = text.indexOf(word);
    EXPECT_GE(index, 0) << word.toStdString();
    return index + static_cast<int>(word.size()) / 2;
}

QStringList selected(const QString& text, int position)
{
    QStringList result;
    for (const Range& r : regionsAround(text, position))
        result << text.mid(r.start, r.length());
    return result;
}
} // namespace

TEST(StcTagRegions, ContentOfTheHeader)
{
    const QString text = "[h1]Standardowe formatowanie[/h1]";
    EXPECT_EQ(selected(text, inside(text, "Standardowe")), QStringList{"Standardowe formatowanie"});
}

TEST(StcTagRegions, ValueOfTheAttributeThenOuterTags)
{
    const QString text = "[div]Intro [a href=\"https://fmt.dev/\" name=\"biblioteką fmt\"] (licencja MIT) end[/div]";
    EXPECT_EQ(selected(text, inside(text, "biblioteką")),
              (QStringList{"biblioteką fmt", "Intro [a href=\"https://fmt.dev/\" name=\"biblioteką fmt\"] (licencja MIT) end"}));
    EXPECT_EQ(selected(text, inside(text, "fmt.dev")).first(), "https://fmt.dev/");
}

TEST(StcTagRegions, NestedTagsInnermostFirst)
{
    const QString text = "[b]bold [i]italic[/i] more[/b]";
    EXPECT_EQ(selected(text, inside(text, "italic")), (QStringList{"italic", "bold [i]italic[/i] more"}));
    EXPECT_EQ(selected(text, inside(text, "more")), QStringList{"bold [i]italic[/i] more"});
}

TEST(StcTagRegions, NeighbouringTagsOnOneLine)
{
    const QString text = "w [b]C++20[/b] pojawił się [cpp]std::format[/cpp], a w [b]C++23[/b] print";
    EXPECT_EQ(selected(text, inside(text, "C++20")), QStringList{"C++20"});
    EXPECT_EQ(selected(text, inside(text, "std::format")), QStringList{"std::format"});
    EXPECT_EQ(selected(text, inside(text, "C++23")), QStringList{"C++23"});
}

TEST(StcTagRegions, TextOutsideOfTagsHasNoRegion)
{
    const QString text = "Plain text [b]bold[/b] plain again";
    EXPECT_TRUE(regionsAround(text, inside(text, "Plain")).isEmpty());
    EXPECT_TRUE(regionsAround(text, inside(text, "again")).isEmpty());
}

TEST(StcTagRegions, ClickInTheTagItselfSelectsItsContent)
{
    const QString text = "[h1]Title[/h1]";
    EXPECT_EQ(selected(text, 2), QStringList{"Title"});                    // on "h1"
    EXPECT_EQ(selected(text, text.indexOf("[/h1]") + 2), QStringList{"Title"}); // on the closing tag
}

TEST(StcTagRegions, CodeBlockIsVerbatim)
{
    const QString text = "[cpp]int a[i] = b[0]; [/cpp] and [b]bold[/b]";
    EXPECT_EQ(selected(text, inside(text, "int a")), QStringList{"int a[i] = b[0]; "});
    EXPECT_EQ(selected(text, inside(text, "bold")), QStringList{"bold"});
}

TEST(StcTagRegions, MultiLineBlock)
{
    const QString text = "[cpp]\nint main()\n{\n}\n[/cpp]\n[b]x[/b]";
    EXPECT_EQ(selected(text, inside(text, "main")), QStringList{"\nint main()\n{\n}\n"});
}

TEST(StcTagRegions, UnclosedAndUnpairedTagsDoNotBreakOthers)
{
    const QString text = "[b]open [i]never closed [u]under[/u] tail[/b] [img src=\"x.png\" alt=\"A picture\"]";
    EXPECT_EQ(selected(text, inside(text, "under")), (QStringList{"under", "open [i]never closed [u]under[/u] tail"}));
    EXPECT_EQ(selected(text, inside(text, "A picture")), QStringList{"A picture"});
}

TEST(StcTagRegions, BracketsWhichAreNotTags)
{
    const QString text = "x[0] = [=] i[i++] [b]ok[/b] [ [b]two[/b]";
    EXPECT_EQ(selected(text, inside(text, "ok")), QStringList{"ok"});
    EXPECT_EQ(selected(text, inside(text, "two")), QStringList{"two"});
}

TEST(StcTagRegions, QuotesWithApostropheAndBracketInValue)
{
    const QString text = "[a href=\"u\" name=\"Bob's [x] page\"] z";
    EXPECT_EQ(selected(text, inside(text, "Bob")), QStringList{"Bob's [x] page"});
}

TEST(StcTagRegions, TagNamesAreCaseInsensitive)
{
    const QString text = "[B]Bold[/b]";
    EXPECT_EQ(selected(text, inside(text, "Bold")), QStringList{"Bold"});
}

TEST(StcTagRegions, EmptyContentIsNotARegion)
{
    const QString text = "[b][/b]";
    EXPECT_TRUE(regionsAround(text, 3).isEmpty());
}
