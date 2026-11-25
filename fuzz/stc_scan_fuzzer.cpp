// Fuzz target for the STC tag scanner (utils/StcTagScanner.cpp).

#include <QString>

#include "utils/StcTagScanner.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    constexpr size_t maxInput = 256u << 10; // 256 KiB
    if (size > maxInput)
        return 0;

    const QString text = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(size));

    using namespace StcTagScanner;
    (void)containsVerbatimTag(text);

    const Scan scan = StcTagScanner::scan(text);

    // matchingTag() is normally fed with the range of a tag name or a whole tag;
    // try it on the ranges the scan found plus a few arbitrary ones.
    for (const Pair& pair : scan.pairs)
    {
        (void)matchingTag(text, pair.openName);
        (void)matchingTag(text, pair.closeName);
        (void)matchingTag(text, {pair.open.start, pair.open.end});
        (void)matchingTag(text, {pair.content().start, pair.content().end});
    }
    (void)matchingTag(text, {0, static_cast<int>(text.size())});
    if (!scan.pairs.isEmpty())
        (void)matchingTag(text, {scan.pairs.first().open.start, scan.pairs.last().close.end});

    // parseTagAt() on every '[' in the text (scan() already does this internally,
    // but calling it directly also covers out-of-range / edge positions)
    for (int i = 0; i < text.size(); ++i)
    {
        if (text[i] == QLatin1Char('['))
            (void)parseTagAt(text, i);
    }
    return 0;
}
