#pragma once

#include <QString>
#include <QStringList>
#include <vector>

/// Mapping between the lines of the STC source and the places of the HTML which cpp0x.pl renders from it.
///
/// The HTML has no information where its parts come from (no `data-line` and the like), so the mapping is found by
/// comparing the texts: the characters of the source (without the STC tags) and the characters of the text nodes
/// of the preview are two almost equal sequences, in the same order. They are aligned (see `SyncMap::SyncMap`),
/// so for a line of the source we know which character of which text node of the preview it became.
namespace PreviewSync
{
/// A place in the preview: the character `offset` (UTF-16 unit, as in JavaScript) of the text node number `chunk`.
struct TextPosition
{
    int chunk = -1;
    int offset = 0;

    bool isValid() const { return chunk >= 0; }
    bool operator==(const TextPosition&) const = default;
};

class SyncMap
{
public:
    SyncMap() = default;

    /// @param stcSource       the text sent to cpp0x.pl (lines are separated with '\n')
    /// @param domTextChunks   texts of the text nodes of the rendered preview, in the document order
    ///
    /// Whitespace is ignored on both sides (the HTML collapses it and `<br>` produces no text at all). The STC tags are
    /// removed from the source; the content of `[cpp]`, `[code]`, `[py]` and `[log]` is kept verbatim, and so are the `;`
    /// inside them, while in the rest of `[csv]` the `;` are cell separators and they are not a part of the text.
    /// What is left is aligned greedily: when the characters differ, the nearest place where both sides continue with
    /// the same text is looked for, so the headers which the server adds to code blocks, removed tags,
    /// separators etc. are skipped.
    SyncMap(const QString& stcSource, const QStringList& domTextChunks);

    /// No text of the source was found in the preview (nothing rendered yet, or the texts are completely different)
    bool isEmpty() const { return matchedChars == 0; }

    /// The place in the preview which shows the beginning of the `line` (0-based) of the source.
    /// @param fraction  [0, 1): how far inside the line to go. Long paragraphs are one line of the source, but many lines
    ///                  in the editor and in the preview, so the scroll can be proportional inside them.
    /// A line without any text (empty, only tags) gives the place of the next line which has some.
    TextPosition positionForLine(int line, double fraction = 0.0) const;

    /// Share of the characters of the preview which were found in the source (1.0 - perfect match). For diagnostics and tests.
    double matchedRatio() const { return domCharCount == 0 ? 0.0 : double(matchedChars) / double(domCharCount); }

private:
    struct DomChar
    {
        int chunk;
        int offset;
    };

    TextPosition positionOfDomChar(int domIndex) const;

    std::vector<DomChar> domChars;       ///< the not-whitespace characters of the preview
    std::vector<int> firstDomOfLine;     ///< for each line of the source: the first aligned character of the preview (or -1)
    std::vector<int> lastDomOfLine;      ///< ... and the last one
    int matchedChars = 0;
    int domCharCount = 0;
};
} // namespace PreviewSync
