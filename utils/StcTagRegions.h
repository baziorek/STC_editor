#pragma once

#include <QList>
#include <QString>

/// Finding the text which the 4th, 5th... click of the left mouse button selects in the STC markup:
///  - the value of the attribute the click is in: `[a href="https://x.y" name="some text"]` -> `some text`,
///  - the text between the tags in which the click is: `[h1]Some title[/h1]` -> `Some title`.
namespace StcTagRegions
{
/// Half-open range of the characters of the text: [start, end).
struct Range
{
    int start = 0;
    int end = 0;

    int length() const { return end - start; }
    bool operator==(const Range&) const = default;
};

/// The regions which contain the cursor `position` (a position between characters, as in `QTextCursor`),
/// from the innermost one to the outermost one: the value of the attribute (if the position is in one),
/// then the content of each pair of tags around the position. No region is repeated.
///
/// The content of `[cpp]`, `[code]`, `[py]` and `[log]` is verbatim, so `a[i]` in a program is not taken for a tag.
/// Tags without the closing tag (`[a href=...]`, `[img ...]`) have no content, but their attributes are found.
QList<Range> regionsAround(const QString& text, int position);
} // namespace StcTagRegions
