// Fuzz target for the line search (utils/LineSearch.cpp).
// The whole input is both the line to search and the source of the query words.

#include <QString>

#include "utils/LineSearch.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    constexpr size_t maxInput = 256u << 10; // 256 KiB
    if (size > maxInput)
        return 0;

    const QString line = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(size));
    const QStringList words = LineSearch::splitIntoWords(line);

    for (const QString& word : words)
        (void)LineSearch::findOccurrences(line, word);

    for (const auto& criteria : {LineSearch::Criteria{},
                                 LineSearch::Criteria{.caseSensitive = true, .wholeWords = false},
                                 LineSearch::Criteria{.caseSensitive = false, .wholeWords = true},
                                 LineSearch::Criteria{.caseSensitive = true, .wholeWords = true}})
    {
        (void)LineSearch::findAllWords(line, words, criteria);
    }
    return 0;
}
