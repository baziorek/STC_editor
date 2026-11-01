#pragma once

#include <optional>
#include <QList>
#include <QString>

/// What the STC markup looks like to the code which has to find its tags: one place for reading a tag, for the tags with
/// verbatim content, and for pairing opening tags with closing ones. (The folding, the selection of the region on a
/// click and the highlighting of the matching tag all use it, so none of them has its own regular expression.)
namespace StcTagScanner
{
/// Half-open range of the characters of the text: [start, end).
struct Range
{
    int start = 0;
    int end = 0;

    int length() const { return end - start; }
    bool operator==(const Range&) const = default;
};

/// `[name attr="value" ...]` or `[/name]`
struct Token
{
    bool closing = false;
    QString name;                 ///< lower case
    Range range;                  ///< the whole `[...]`
    Range nameRange;              ///< just the name, as it is written in the text
    QList<Range> attributeValues; ///< between the quotes
};

/// `[cpp]`, `[code]`, `[py]` and `[log]`: what is inside is code, not markup (`a[i]` is not a tag, `[h1]` starts nothing)
bool isVerbatimTag(const QString& name);

/// The names of the verbatim tags joined with `|`, for a regular expression which has to know them: "cpp|code|py|log"
QString verbatimTagsAlternation();

/// Reads the tag which starts with the `[` at `start`. Not a tag (`[0]`, `[i++]`, `[=]`, an unfinished one, one which
/// spans lines): std::nullopt.
std::optional<Token> parseTagAt(const QString& text, int start);

/// Whether the text has a verbatim tag, opening or closing, anywhere
bool containsVerbatimTag(const QString& text);

/// An opening tag and the closing one which belongs to it
struct Pair
{
    Range open;  ///< the tags, with the brackets
    Range close;
    Range openName; ///< the names inside of them
    Range closeName;

    Range content() const { return {open.end, close.start}; }
};

struct Scan
{
    QList<Token> openingTags; ///< all, also these which have no closing tag (`[a href=...]`, `[img src=...]`)
    QList<Pair> pairs;
};

/// Finds the tags of the text and pairs them: a closing tag belongs to the nearest opening tag of its name, the tags
/// opened in between had no closing tag. The content of a verbatim tag is skipped. Tags without a closing tag are
/// only in `openingTags`.
Scan scan(const QString& text);

/// The other tag of the pair when `selection` is exactly the name of a tag (`h1` in `[h1]`) or the whole tag (`[h1]`):
/// the name of the closing tag for an opening one and the other way round. std::nullopt when the selection is
/// anything else (an attribute, a word of the text) or when the tag has no pair (`[a href=...]`, `[img src=...]`).
std::optional<Range> matchingTag(const QString& text, Range selection);
} // namespace StcTagScanner
