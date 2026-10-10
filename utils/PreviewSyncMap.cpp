#include "PreviewSyncMap.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace PreviewSync
{
namespace
{
/// A character of the source which is a part of the text shown in the preview, and the line (0-based) it is in
struct SourceChar
{
    char16_t character;
    int line;
};

/// Longest skip, on one side, which is looked for with the cheap search of the next common text
constexpr int kNearWindow = 160;
/// How many equal characters make a "common text" which both sides continue with
constexpr int kNearAnchorLength = 8;
/// ... within a short skip (a separator, a short label), a shorter text is enough
constexpr int kShortSkip = 12;
constexpr int kShortAnchorLength = 4;
/// The search over the whole text (when the near one found nothing): the length of the text, and how far it goes
constexpr int kFarAnchorLength = 12;
constexpr int kFarSourceWindow = 4000;
constexpr int kFarDomWindow = 30000;

bool isVerbatimTag(const QString& name)
{
    return name == "cpp" || name == "code" || name == "py" || name == "log";
}

const QSet<QString>& knownTags()
{
    static const QSet<QString> tags = {"run", "cpp", "py", "code", "log", "div", "a", "pkt", "csv", "b", "i", "u", "s",
                                       "h1", "h2", "h3", "h4", "cytat", "sub", "sup", "tt", "img"};
    return tags;
}

struct TagMatch
{
    int length = 0; ///< 0 - there is no known STC tag here
    QString name;
    bool closing = false;
    QString attributes; ///< the text after the name in an opening tag
};

/// `text[position]` is '['. Recognizes `[name]`, `[name attributes]` and `[/name]` of the tags known to STC;
/// anything else (`[0]`, `[i + 1]`, `[unknown]`) is not a tag.
TagMatch parseTagAt(const QString& text, int position)
{
    const int size = static_cast<int>(text.size());
    int i = position + 1;

    TagMatch result;
    if (i < size && text[i] == '/')
    {
        result.closing = true;
        ++i;
    }

    const int nameStart = i;
    while (i < size && (text[i].unicode() < 128) && (text[i].isLetterOrNumber() || text[i] == '_'))
        ++i;
    result.name = text.mid(nameStart, i - nameStart).toLower();
    if (!knownTags().contains(result.name))
        return {};

    if (i < size && text[i] == ']')
    {
        result.length = i + 1 - position;
        return result;
    }

    if (result.closing || i >= size || !text[i].isSpace())
        return {};

    // attributes, up to the first ']' which is not in quotes; a tag does not go over the end of the line
    const int attributesStart = i;
    bool inQuotes = false;
    for (; i < size && text[i] != '\n'; ++i)
    {
        if (text[i] == '"')
            inQuotes = !inQuotes;
        else if (text[i] == ']' && !inQuotes)
        {
            result.length = i + 1 - position;
            result.attributes = text.mid(attributesStart, i - attributesStart);
            return result;
        }
    }
    return {};
}

/// `[a href="https://x.y" name="some text"]` shows `some text`, and without the name - the address
QString textShownByLink(const QString& attributes)
{
    static const QRegularExpression name(R"re(\bname\s*=\s*"([^"]*)")re");
    static const QRegularExpression href(R"re(\bhref\s*=\s*"([^"]*)")re");

    if (const auto match = name.match(attributes); match.hasMatch())
        return match.captured(1);
    if (const auto match = href.match(attributes); match.hasMatch())
        return match.captured(1);
    return {};
}

/// The characters of the source which the server turns into the text of the page, with the lines they are in.
/// Whitespace is skipped: in HTML it is collapsed, and `<br>` has no text, so only the other characters can be compared.
std::vector<SourceChar> visibleSourceChars(const QString& source)
{
    std::vector<SourceChar> result;
    result.reserve(static_cast<size_t>(source.size()));

    int line = 0;
    int csvDepth = 0;       // inside [csv]...[/csv] the ';' separates cells
    QString verbatimTag;    // inside [cpp], [code], [py], [log] only the closing tag of the block is a tag

    const int size = static_cast<int>(source.size());
    for (int i = 0; i < size; ++i)
    {
        const QChar c = source[i];

        if (c == '\n')
        {
            ++line;
            continue;
        }

        if (c == '[')
        {
            const TagMatch tag = parseTagAt(source, i);
            const bool recognized = tag.length > 0 && (verbatimTag.isEmpty() || (tag.closing && tag.name == verbatimTag));
            if (recognized)
            {
                if (tag.closing && tag.name == verbatimTag)
                    verbatimTag.clear();
                else if (!tag.closing && isVerbatimTag(tag.name))
                    verbatimTag = tag.name;
                else if (tag.name == "csv")
                    csvDepth += tag.closing ? -1 : 1;
                else if (tag.name == "a" && !tag.closing)
                {
                    for (const QChar shown : textShownByLink(tag.attributes))
                        result.push_back({shown.unicode(), line});
                }

                i += tag.length - 1; // the tag itself is not a text
                continue;
            }
        }

        if (c.isSpace())
            continue;

        if (c == ';' && csvDepth > 0 && verbatimTag.isEmpty())
            continue;

        result.push_back({c.unicode(), line});
    }
    return result;
}

/// Positions of the text of the preview, to be looked up by long fragments
class FragmentIndex
{
public:
    explicit FragmentIndex(const std::vector<char16_t>& text) : text(text)
    {
        if (static_cast<int>(text.size()) < kFarAnchorLength)
            return;
        for (int i = 0; i + kFarAnchorLength <= static_cast<int>(text.size()); ++i)
            positions[hashAt(text, i)].push_back(i);
    }

    /// The first place not before `from`, where the `kFarAnchorLength` characters starting at `fragment` are, or -1
    int find(const std::vector<SourceChar>& source, int fragment, int from) const
    {
        const auto found = positions.find(hashOfSource(source, fragment));
        if (found == positions.end())
            return -1;

        const auto& candidates = found->second;
        for (auto it = std::lower_bound(candidates.begin(), candidates.end(), from); it != candidates.end(); ++it)
        {
            bool equal = true;
            for (int k = 0; k < kFarAnchorLength && equal; ++k)
                equal = text[static_cast<size_t>(*it + k)] == source[static_cast<size_t>(fragment + k)].character;
            if (equal)
                return *it;
        }
        return -1;
    }

private:
    static uint64_t hashAt(const std::vector<char16_t>& chars, int start)
    {
        uint64_t hash = 1469598103934665603ull;
        for (int k = 0; k < kFarAnchorLength; ++k)
            hash = (hash ^ chars[static_cast<size_t>(start + k)]) * 1099511628211ull;
        return hash;
    }

    static uint64_t hashOfSource(const std::vector<SourceChar>& chars, int start)
    {
        uint64_t hash = 1469598103934665603ull;
        for (int k = 0; k < kFarAnchorLength; ++k)
            hash = (hash ^ chars[static_cast<size_t>(start + k)].character) * 1099511628211ull;
        return hash;
    }

    const std::vector<char16_t>& text;
    std::unordered_map<uint64_t, std::vector<int>> positions;
};

/// Do the `length` characters of the source from `s` and of the preview from `d` equal?
bool sameText(const std::vector<SourceChar>& source, int s, const std::vector<char16_t>& dom, int d, int length)
{
    if (s + length > static_cast<int>(source.size()) || d + length > static_cast<int>(dom.size()))
        return false;
    for (int k = 0; k < length; ++k)
    {
        if (source[static_cast<size_t>(s + k)].character != dom[static_cast<size_t>(d + k)])
            return false;
    }
    return true;
}

struct Skip
{
    int source = 0; ///< how many characters of the source to skip
    int dom = 0;    ///< how many characters of the preview to skip
};

/// The smallest skip (the sum of the skipped characters) after which both texts continue with `anchorLength` equal characters.
/// Only skips up to `window` characters on each side are considered. Returns nullopt when there is none.
std::optional<Skip> nearestCommonText(const std::vector<SourceChar>& source, int s,
                                      const std::vector<char16_t>& dom, int d,
                                      int window, int anchorLength)
{
    for (int total = 1; total <= 2 * window; ++total)
    {
        const int fromSource = std::max(0, total - window);
        const int toSource = std::min(total, window);
        for (int skippedSource = fromSource; skippedSource <= toSource; ++skippedSource)
        {
            const int skippedDom = total - skippedSource;
            if (sameText(source, s + skippedSource, dom, d + skippedDom, anchorLength))
                return Skip{skippedSource, skippedDom};
        }
    }
    return std::nullopt;
}

/// Alignment of the two texts, which are in the same order and differ in small places (see SyncMap::SyncMap).
/// @return for each character of the preview: the line of the source which it comes from, or -1
std::vector<int> alignLines(const std::vector<SourceChar>& source, const std::vector<char16_t>& dom)
{
    std::vector<int> lineOfDomChar(dom.size(), -1);
    std::optional<FragmentIndex> farIndex; // built when the first long jump is needed

    const int sourceSize = static_cast<int>(source.size());
    const int domSize = static_cast<int>(dom.size());

    int s = 0;
    int d = 0;
    while (s < sourceSize && d < domSize)
    {
        if (source[static_cast<size_t>(s)].character == dom[static_cast<size_t>(d)])
        {
            lineOfDomChar[static_cast<size_t>(d)] = source[static_cast<size_t>(s)].line;
            ++s;
            ++d;
            continue;
        }

        // The texts differ: skip what is only on one side, and continue from the place where both texts are equal again
        std::optional<Skip> skip = nearestCommonText(source, s, dom, d, kNearWindow, kNearAnchorLength);
        if (!skip)
            skip = nearestCommonText(source, s, dom, d, kShortSkip, kShortAnchorLength);

        if (!skip)
        {
            if (!farIndex)
                farIndex.emplace(dom);

            for (int ahead = 0; ahead < kFarSourceWindow && s + ahead + kFarAnchorLength <= sourceSize; ++ahead)
            {
                const int found = farIndex->find(source, s + ahead, d);
                if (found >= 0 && found - d <= kFarDomWindow)
                {
                    skip = Skip{ahead, found - d};
                    break;
                }
            }
        }

        if (skip)
        {
            s += skip->source;
            d += skip->dom;
        }
        else
        {
            ++s; // nothing in common is near: take it for a replaced character
            ++d;
        }
    }
    return lineOfDomChar;
}
} // namespace


SyncMap::SyncMap(const QString& stcSource, const QStringList& domTextChunks)
{
    std::vector<char16_t> domText;
    for (int chunk = 0; chunk < domTextChunks.size(); ++chunk)
    {
        const QString& text = domTextChunks[chunk];
        for (int offset = 0; offset < text.size(); ++offset)
        {
            if (text[offset].isSpace())
                continue;
            domText.push_back(text[offset].unicode());
            domChars.push_back({chunk, offset});
        }
    }
    domCharCount = static_cast<int>(domChars.size());

    const std::vector<SourceChar> source = visibleSourceChars(stcSource);
    const std::vector<int> lineOfDomChar = alignLines(source, domText);

    const int lineCount = static_cast<int>(stcSource.count('\n')) + 1;
    firstDomOfLine.assign(static_cast<size_t>(lineCount), -1);
    lastDomOfLine.assign(static_cast<size_t>(lineCount), -1);
    for (int d = 0; d < domCharCount; ++d)
    {
        const int line = lineOfDomChar[static_cast<size_t>(d)];
        if (line < 0)
            continue;

        ++matchedChars;
        if (firstDomOfLine[static_cast<size_t>(line)] < 0)
            firstDomOfLine[static_cast<size_t>(line)] = d;
        lastDomOfLine[static_cast<size_t>(line)] = d;
    }
}

TextPosition SyncMap::positionForLine(int line, double fraction) const
{
    if (isEmpty() || firstDomOfLine.empty())
        return {};

    line = std::clamp(line, 0, static_cast<int>(firstDomOfLine.size()) - 1);

    // lines without text (empty, only tags) show where the next line with text is
    int withText = line;
    while (withText < static_cast<int>(firstDomOfLine.size()) && firstDomOfLine[static_cast<size_t>(withText)] < 0)
        ++withText;

    if (withText == static_cast<int>(firstDomOfLine.size()))
    {
        // after the last line with text
        withText = line;
        while (withText > 0 && firstDomOfLine[static_cast<size_t>(withText)] < 0)
            --withText;
        return positionOfDomChar(lastDomOfLine[static_cast<size_t>(withText)]);
    }

    const int first = firstDomOfLine[static_cast<size_t>(withText)];
    if (withText != line)
        return positionOfDomChar(first);

    const int last = lastDomOfLine[static_cast<size_t>(withText)];
    const double clamped = std::clamp(fraction, 0.0, 0.999999);
    return positionOfDomChar(first + static_cast<int>(clamped * (last - first + 1)));
}

TextPosition SyncMap::positionOfDomChar(int domIndex) const
{
    if (domIndex < 0 || domIndex >= static_cast<int>(domChars.size()))
        return {};
    const DomChar& domChar = domChars[static_cast<size_t>(domIndex)];
    return {domChar.chunk, domChar.offset};
}
} // namespace PreviewSync
