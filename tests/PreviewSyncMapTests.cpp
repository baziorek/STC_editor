#include <gtest/gtest.h>
#include "utils/PreviewSyncMap.h"

using PreviewSync::SyncMap;
using PreviewSync::TextPosition;

namespace
{
/// The number of the text node (of the preview) which shows the beginning of the line
int chunkOfLine(const SyncMap& map, int line, double fraction = 0.0)
{
    return map.positionForLine(line, fraction).chunk;
}
} // namespace

TEST(PreviewSyncMap, HeadersAndParagraphsAreFoundWithoutTags)
{
    const QString source =
        "[h2]Podstawy[/h2]\n"
        "[h3]1. Pierwszy program[/h3]\n"
        "Zwykły akapit z [b]pogrubieniem[/b] w środku.\n";
    const QStringList dom = {"Podstawy", "1. Pierwszy program", "Zwykły akapit z ", "pogrubieniem", " w środku."};

    const SyncMap map(source, dom);

    EXPECT_FALSE(map.isEmpty());
    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
    EXPECT_EQ(chunkOfLine(map, 0), 0);
    EXPECT_EQ(chunkOfLine(map, 1), 1);
    EXPECT_EQ(chunkOfLine(map, 2), 2);
}

TEST(PreviewSyncMap, WhitespaceDoesNotMatter)
{
    // the server joins lines with <br>, a non-breaking space replaces a space before a tag
    const QString source = "pierwsza linia\ndruga   linia\n";
    const QStringList dom = {"pierwsza", "linia", "druga linia"};

    const SyncMap map(source, dom);

    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
    EXPECT_EQ(chunkOfLine(map, 0), 0);
    EXPECT_EQ(chunkOfLine(map, 1), 2);
}

TEST(PreviewSyncMap, OffsetInsideTheTextNode)
{
    const QString source = "Jakiś wstęp\n[b]pogrubienie[/b] a potem zwykły tekst\n";
    const QStringList dom = {"Jakiś wstęp", "pogrubienie", " a potem zwykły tekst"};

    const SyncMap map(source, dom);

    EXPECT_EQ(map.positionForLine(1), (TextPosition{1, 0}));
}

TEST(PreviewSyncMap, TextAddedByTheServerIsSkipped)
{
    // cpp0x.pl puts the name of the language over every block of code
    const QString source =
        "Przed kodem\n"
        "[cpp]\n"
        "int main()\n"
        "{\n"
        "}\n"
        "[/cpp]\n"
        "Po kodzie\n";
    const QStringList dom = {"Przed kodem", "C/C++", "int ", "main", "()", "{", "}", "Po kodzie"};

    const SyncMap map(source, dom);

    EXPECT_EQ(chunkOfLine(map, 0), 0);
    EXPECT_EQ(chunkOfLine(map, 2), 2);
    EXPECT_EQ(chunkOfLine(map, 3), 5);
    EXPECT_EQ(chunkOfLine(map, 6), 7);
}

TEST(PreviewSyncMap, CsvSeparatorsAreNotText)
{
    const QString source =
        "[csv extended header]C++;python3;\n"
        "[run]raz[/run];[run]jeden[/run];\n"
        "[run]dwa[/run];[run]dwa i pol[/run];\n"
        "[/csv]\n"
        "Koniec\n";
    const QStringList dom = {"C++", "python3", "raz", "jeden", "dwa", "dwa i pol", "Koniec"};

    const SyncMap map(source, dom);

    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
    EXPECT_EQ(chunkOfLine(map, 1), 2);
    EXPECT_EQ(chunkOfLine(map, 2), 4);
    EXPECT_EQ(chunkOfLine(map, 4), 6);
}

TEST(PreviewSyncMap, SemicolonsInCodeInsideCsvAreText)
{
    const QString source = "[csv]\n[run][cpp]int a = 1;[/cpp][/run];[run]ok[/run];\n[/csv]\n";
    const QStringList dom = {"int a = 1;", "ok"};

    const SyncMap map(source, dom);

    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
}

TEST(PreviewSyncMap, SquareBracketsInCodeAreNotTags)
{
    const QString source = "[cpp]\nv[i] = w[b];\n[/cpp]\nnastępny\n";
    const QStringList dom = {"v[i] = w[b];", "następny"};

    const SyncMap map(source, dom);

    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
    EXPECT_EQ(chunkOfLine(map, 3), 1);
}

TEST(PreviewSyncMap, LinkShowsItsNameOrItsAddress)
{
    const QString source =
        "Zobacz [a href=\"https://x.y/z\" name=\"stronę\"] oraz\n"
        "[a href=\"https://a.b/c\"]\n"
        "i koniec\n";
    const QStringList dom = {"Zobacz ", "stronę", " oraz", "https://a.b/c", "i koniec"};

    const SyncMap map(source, dom);

    EXPECT_DOUBLE_EQ(map.matchedRatio(), 1.0);
    EXPECT_EQ(chunkOfLine(map, 1), 3);
    EXPECT_EQ(chunkOfLine(map, 2), 4);
}

TEST(PreviewSyncMap, LineWithoutTextShowsTheNextLine)
{
    const QString source = "pierwszy\n\n[div]\n\ndrugi\n";
    const QStringList dom = {"pierwszy", "drugi"};

    const SyncMap map(source, dom);

    EXPECT_EQ(chunkOfLine(map, 1), 1);
    EXPECT_EQ(chunkOfLine(map, 2), 1);
    EXPECT_EQ(chunkOfLine(map, 3), 1);
}

TEST(PreviewSyncMap, LinesAfterTheLastTextShowTheEnd)
{
    const QString source = "tekst\n\n\n";
    const QStringList dom = {"tekst"};

    const SyncMap map(source, dom);

    EXPECT_EQ(chunkOfLine(map, 3), 0);
    EXPECT_EQ(chunkOfLine(map, 100), 0) << "a line out of range is the last one";
    EXPECT_EQ(chunkOfLine(map, -5), 0);
}

TEST(PreviewSyncMap, FractionGoesInsideALongLine)
{
    const QString source = "aaaaaaaaaabbbbbbbbbbcccccccccc\n";
    const QStringList dom = {"aaaaaaaaaa", "bbbbbbbbbb", "cccccccccc"};

    const SyncMap map(source, dom);

    EXPECT_EQ(map.positionForLine(0, 0.0), (TextPosition{0, 0}));
    EXPECT_EQ(map.positionForLine(0, 0.5), (TextPosition{1, 5}));
    EXPECT_EQ(map.positionForLine(0, 0.99), (TextPosition{2, 9}));
}

TEST(PreviewSyncMap, LongDifferencesDoNotLoseTheLaterText)
{
    // 300 characters which are only in the preview, and 500 which are only in the source
    const QString onlyInPreview(300, QChar('x'));
    const QString onlyInSource(500, QChar('y'));
    const QString source = "początek tekstu\n" + onlyInSource + "\nśrodek tekstu i jeszcze coś\nkoniec dokumentu\n";
    const QStringList dom = {"początek tekstu", onlyInPreview, "środek tekstu i jeszcze coś", "koniec dokumentu"};

    const SyncMap map(source, dom);

    EXPECT_EQ(chunkOfLine(map, 0), 0);
    EXPECT_EQ(chunkOfLine(map, 2), 2);
    EXPECT_EQ(chunkOfLine(map, 3), 3);
}

TEST(PreviewSyncMap, RepeatedTextsDoNotConfuseTheOrder)
{
    const QString source = "a\nb\na\nb\na\nb\n";
    const QStringList dom = {"a", "b", "a", "b", "a", "b"};

    const SyncMap map(source, dom);

    for (int line = 0; line < 6; ++line)
        EXPECT_EQ(chunkOfLine(map, line), line);
}

TEST(PreviewSyncMap, NothingRenderedYet)
{
    const SyncMap noPreview("jakiś tekst\n", {});
    EXPECT_TRUE(noPreview.isEmpty());
    EXPECT_FALSE(noPreview.positionForLine(0).isValid());

    const SyncMap defaultMap;
    EXPECT_TRUE(defaultMap.isEmpty());
    EXPECT_FALSE(defaultMap.positionForLine(0).isValid());

    const SyncMap noSource("", {"coś"});
    EXPECT_TRUE(noSource.isEmpty());
}
