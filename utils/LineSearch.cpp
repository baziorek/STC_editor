#include "LineSearch.h"

#include <QStringView>
#include <algorithm>
#include <QChar>
#include <Qt>
#include <QtGlobal>

namespace LineSearch
{
QStringList splitIntoWords(const QString& query)
{
    // simplified() turns every run of white space (also tabs and no-break spaces) into one space and trims the ends
    QStringList words;
    for (const QString& word : query.simplified().split(u' ', Qt::SkipEmptyParts))
    {
        if (!words.contains(word))
            words.append(word);
    }
    return words;
}

namespace
{
bool isWholeWordAt(const QString& line, qsizetype start, qsizetype end)
{
    const bool leftBoundary = start == 0 || !line[start - 1].isLetterOrNumber();
    const bool rightBoundary = end >= line.size() || !line[end].isLetterOrNumber();
    return leftBoundary && rightBoundary;
}
} // namespace

QList<Occurrence> findOccurrences(const QString& line, const QString& word)
{
    QList<Occurrence> places;
    if (word.isEmpty())
        return places;

    qsizetype from = 0;
    for (;;)
    {
        const qsizetype start = line.indexOf(word, from, Qt::CaseInsensitive);
        if (start < 0)
            break;

        const qsizetype end = start + word.size();
        Occurrence place;
        place.start = static_cast<int>(start);
        place.length = static_cast<int>(word.size());
        place.sameCase = QStringView(line).sliced(start, word.size()) == QStringView(word);
        place.wholeWord = isWholeWordAt(line, start, end);
        places.append(place);

        from = end; // the next place may not overlap this one
    }
    return places;
}

QList<Occurrence> findAllWords(const QString& line, const QStringList& words, const Criteria& criteria)
{
    QList<Occurrence> all;
    if (words.isEmpty())
        return all;

    for (const QString& word : words)
    {
        const qsizetype placesBefore = all.size();
        for (const Occurrence& place : findOccurrences(line, word))
        {
            if (place.fits(criteria))
                all.append(place);
        }

        if (all.size() == placesBefore) // this word is not in the line, so the line does not match
            return {};
    }

    std::stable_sort(all.begin(), all.end(), [](const Occurrence& left, const Occurrence& right) {
        return left.start < right.start;
    });
    return all;
}
} // namespace LineSearch
