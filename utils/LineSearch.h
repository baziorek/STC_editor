#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/// Searching a line of text for words.
///
/// One word is a plain piece of text, as the find widget always searched. Several words may stand in any order and
/// at any distance from each other: `Ala ma koty` finds the line `Ale lubi ryby i koty ta Ala co ma psa`.
namespace LineSearch
{
/// What the user asked for with the check boxes of the find widget.
struct Criteria
{
    bool caseSensitive = false;
    bool wholeWords = false;
};

/// One place of a line in which a word was found.
struct Occurrence
{
    int start = 0;           ///< position in the line
    int length = 0;
    bool sameCase = false;   ///< the text in the line is letter for letter the word, not only equal to it up to the case
    bool wholeWord = false;  ///< neither side of the place touches a letter or a digit

    int end() const { return start + length; }

    /// Is the place good for the user, who asked for the `criteria`?
    bool fits(const Criteria& criteria) const
    {
        return (!criteria.caseSensitive || sameCase) && (!criteria.wholeWords || wholeWord);
    }
};

/// The words of the query: the pieces between any white space. Each word is there once.
QStringList splitIntoWords(const QString& query);

/// All places of the line in which the word is, from left to right, not overlapping each other.
///
/// The word is looked for up to the case, and each place tells whether it was found exactly (`Occurrence::sameCase`)
/// and whether it is a whole word, so one search serves all the combinations of the check boxes.
QList<Occurrence> findOccurrences(const QString& line, const QString& word);

/// The places of all the `words` in the line, from left to right, but only when each word has at least one place which
/// fits the `criteria`. Otherwise the line does not match and the result is empty.
///
/// Places of different words may overlap (the words `ala` and `al` are both in `Ala`).
QList<Occurrence> findAllWords(const QString& line, const QStringList& words, const Criteria& criteria);
} // namespace LineSearch
