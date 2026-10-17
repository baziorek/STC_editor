#include <gtest/gtest.h>
#include "utils/LineSearch.h"

using LineSearch::Criteria;
using LineSearch::findAllWords;
using LineSearch::findOccurrences;
using LineSearch::Occurrence;
using LineSearch::splitIntoWords;

namespace
{
/// The texts of the places, in the order in which they stand in the line
QStringList textsOf(const QString& line, const QList<Occurrence>& places)
{
    QStringList texts;
    for (const Occurrence& place : places)
        texts << line.mid(place.start, place.length);
    return texts;
}

Criteria sensitive() { return {.caseSensitive = true, .wholeWords = false}; }
Criteria whole() { return {.caseSensitive = false, .wholeWords = true}; }
} // namespace

// ------------------------------------------------------------------ splitting the query into words

TEST(LineSearchWords, SplitsOnWhiteSpace)
{
    EXPECT_EQ(splitIntoWords("Ala ma koty"), (QStringList{"Ala", "ma", "koty"}));
    EXPECT_EQ(splitIntoWords("  Ala\t ma \n  koty  "), (QStringList{"Ala", "ma", "koty"}));
    EXPECT_EQ(splitIntoWords(QString("Ala ma")), (QStringList{"Ala", "ma"})); // no-break space
}

TEST(LineSearchWords, OneWordStaysOneWord)
{
    EXPECT_EQ(splitIntoWords("std::cout"), QStringList{"std::cout"});
    EXPECT_EQ(splitIntoWords("  std::cout  "), QStringList{"std::cout"});
}

TEST(LineSearchWords, NothingToSearchFor)
{
    EXPECT_TRUE(splitIntoWords("").isEmpty());
    EXPECT_TRUE(splitIntoWords(" \t ").isEmpty());
}

TEST(LineSearchWords, RepeatedWordIsKeptOnce)
{
    EXPECT_EQ(splitIntoWords("ma ala ma"), (QStringList{"ma", "ala"}));
}

// ------------------------------------------------------------------ places of one word

TEST(LineSearchOccurrences, PlacesAreFoundFromLeftToRight)
{
    const QString line = "koty i koty i psy";
    const QList<Occurrence> places = findOccurrences(line, "koty");
    ASSERT_EQ(places.size(), 2);
    EXPECT_EQ(places[0].start, 0);
    EXPECT_EQ(places[1].start, 7);
    EXPECT_EQ(places[1].end(), 11);
}

TEST(LineSearchOccurrences, PlacesDoNotOverlap)
{
    EXPECT_EQ(findOccurrences("aaaa", "aa").size(), 2); // 0..2 and 2..4, not three
    EXPECT_EQ(findOccurrences("aaa", "aa").size(), 1);
}

TEST(LineSearchOccurrences, NoWordNoPlaces)
{
    EXPECT_TRUE(findOccurrences("anything", "").isEmpty());
    EXPECT_TRUE(findOccurrences("", "x").isEmpty());
    EXPECT_TRUE(findOccurrences("kot", "pies").isEmpty());
}

TEST(LineSearchOccurrences, TellsWhetherTheCaseWasTheSame)
{
    const QList<Occurrence> places = findOccurrences("Ala ala ALA", "ala");
    ASSERT_EQ(places.size(), 3);
    EXPECT_FALSE(places[0].sameCase);
    EXPECT_TRUE(places[1].sameCase);
    EXPECT_FALSE(places[2].sameCase);
}

TEST(LineSearchOccurrences, TellsWhetherItIsAWholeWord)
{
    const QString line = "ala alabama bala (ala) ala1 ala_x";
    const QList<Occurrence> places = findOccurrences(line, "ala");
    ASSERT_EQ(places.size(), 6);
    EXPECT_TRUE(places[0].wholeWord);   // ala
    EXPECT_FALSE(places[1].wholeWord);  // alabama
    EXPECT_FALSE(places[2].wholeWord);  // bala
    EXPECT_TRUE(places[3].wholeWord);   // (ala)
    EXPECT_FALSE(places[4].wholeWord);  // ala1: a digit is a part of a word
    EXPECT_TRUE(places[5].wholeWord);   // ala_x: underscore does not glue words, as in QTextDocument::find
}

TEST(LineSearchOccurrences, FitsTheCriteria)
{
    const QList<Occurrence> places = findOccurrences("Alabama ALA", "ala");
    ASSERT_EQ(places.size(), 2);
    EXPECT_TRUE(places[0].fits({}));
    EXPECT_FALSE(places[0].fits(sensitive()));
    EXPECT_FALSE(places[0].fits(whole()));   // inside "Alabama"
    EXPECT_TRUE(places[1].fits(whole()));
    EXPECT_FALSE(places[1].fits(sensitive()));
}

TEST(LineSearchOccurrences, PolishLettersIgnoreCaseToo)
{
    const QList<Occurrence> places = findOccurrences("Łąka jest zielona", "ŁĄKA");
    ASSERT_EQ(places.size(), 1);
    EXPECT_FALSE(places[0].sameCase);
    EXPECT_EQ(places[0].start, 0);
}

// ------------------------------------------------------------------ several words in a line

TEST(LineSearchAllWords, WordsInAnyOrderFindTheLine)
{
    const QString line = "Ale lubi ryby i koty ta Ala co ma psa";
    const QList<Occurrence> places = findAllWords(line, splitIntoWords("Ala ma koty"), {});
    // The query has the words in the order Ala, ma, koty, while the line has koty, Ala, ma. The places are in line order.
    EXPECT_EQ(textsOf(line, places), (QStringList{"koty", "Ala", "ma"}));
}

TEST(LineSearchAllWords, EveryOrderOfTheQueryFindsTheSameLine)
{
    const QString line = "Ale lubi ryby i koty ta Ala co ma psa";
    const QStringList queries{"Ala ma koty", "koty Ala ma", "ma koty Ala", "ma Ala koty", "koty ma Ala", "Ala koty ma"};
    for (const QString& query : queries)
        EXPECT_EQ(findAllWords(line, splitIntoWords(query), {}).size(), 3) << query.toStdString();
}

TEST(LineSearchAllWords, MissingWordMeansNoMatch)
{
    EXPECT_TRUE(findAllWords("Ala ma psa", splitIntoWords("Ala ma koty"), {}).isEmpty());
    EXPECT_TRUE(findAllWords("", splitIntoWords("Ala ma"), {}).isEmpty());
}

TEST(LineSearchAllWords, NoWordsMatchNothing)
{
    EXPECT_TRUE(findAllWords("Ala ma koty", {}, {}).isEmpty());
}

TEST(LineSearchAllWords, CaseIsIgnoredUnlessAsked)
{
    const QString line = "Ale lubi koty ta ala";
    EXPECT_FALSE(findAllWords(line, {"Ala", "koty"}, {}).isEmpty());
    EXPECT_TRUE(findAllWords(line, {"Ala", "koty"}, sensitive()).isEmpty()); // there is only "ala"
    EXPECT_FALSE(findAllWords(line, {"ala", "koty"}, sensitive()).isEmpty());
}

TEST(LineSearchAllWords, WholeWordsAreCheckedForEachWord)
{
    const QString line = "Alabama ma koty";
    EXPECT_FALSE(findAllWords(line, {"Ala", "koty"}, {}).isEmpty());
    EXPECT_TRUE(findAllWords(line, {"Ala", "koty"}, whole()).isEmpty());   // "Ala" is only a part of "Alabama"
    EXPECT_FALSE(findAllWords(line, {"ma", "koty"}, whole()).isEmpty());
}

TEST(LineSearchAllWords, ASecondPlaceOfAWordCanSaveTheLine)
{
    // The first "ala" is a part of "alabama", but the second one is a whole word.
    const QString line = "alabama i ala ma koty";
    const QList<Occurrence> places = findAllWords(line, {"ala", "koty"}, whole());
    EXPECT_EQ(textsOf(line, places), (QStringList{"ala", "koty"}));
    EXPECT_EQ(places.first().start, 10);
}

TEST(LineSearchAllWords, ReturnsEveryPlaceOfEveryWord)
{
    const QString line = "ma koty, ma psy, ma ryby";
    const QList<Occurrence> places = findAllWords(line, {"koty", "ma"}, {});
    EXPECT_EQ(textsOf(line, places), (QStringList{"ma", "koty", "ma", "ma"}));
    EXPECT_TRUE(std::is_sorted(places.begin(), places.end(), [](const Occurrence& a, const Occurrence& b) {
        return a.start < b.start;
    }));
}

TEST(LineSearchAllWords, WordsMayOverlap)
{
    const QString line = "Ala";
    EXPECT_EQ(findAllWords(line, {"ala", "al"}, {}).size(), 2);
}

TEST(LineSearchAllWords, PolishLettersWork)
{
    const QString line = "Łąka jest zielona";
    EXPECT_FALSE(findAllWords(line, splitIntoWords("ZIELONA łąka"), {}).isEmpty());
    EXPECT_TRUE(findAllWords(line, splitIntoWords("ZIELONA łąka"), sensitive()).isEmpty());
}

TEST(LineSearchAllWords, OneWordGivesTheSameAsFindingIt)
{
    const QString line = "koty i koty i psy";
    const QList<Occurrence> all = findAllWords(line, {"koty"}, {});
    const QList<Occurrence> one = findOccurrences(line, "koty");
    ASSERT_EQ(all.size(), one.size());
    for (qsizetype i = 0; i < one.size(); ++i)
        EXPECT_EQ(all[i].start, one[i].start);
}
