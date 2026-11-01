#include "StcTagRegions.h"

#include <algorithm>
#include <utility>

QList<StcTagRegions::Range> StcTagRegions::regionsAround(const QString& text, int position)
{
    const StcTagScanner::Scan scanned = StcTagScanner::scan(text);

    QList<Range> regions;
    auto add = [&regions](const Range& range) {
        if (range.length() > 0 && !regions.contains(range))
            regions.append(range);
    };

    // 1. the attribute value in which the position is
    for (const StcTagScanner::Token& token : scanned.openingTags)
    {
        if (position < token.range.start || position > token.range.end)
            continue;
        for (const Range& value : token.attributeValues)
        {
            if (position >= value.start && position <= value.end)
            {
                add(value);
                break;
            }
        }
        break; // the tokens do not overlap
    }

    // 2. the content of the pairs of tags around the position (the position may be in a tag itself), the smallest first
    QList<Range> around;
    for (const StcTagScanner::Pair& pair : scanned.pairs)
    {
        if (position >= pair.open.start && position <= pair.close.end)
            around.append(pair.content());
    }
    std::stable_sort(around.begin(), around.end(), [](const Range& a, const Range& b) { return a.length() < b.length(); });
    for (const Range& range : std::as_const(around))
        add(range);

    return regions;
}
