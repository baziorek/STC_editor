#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/// Which parts of an STC document can be folded (collapsed to one line) in the editor. Only the algorithm, no widget:
/// the lines of the document go in, the regions come out.
///
/// There are three kinds of regions:
///  - a section of a header: from `[hN]` to the line before the next header of the same or a shallower level
///    (`[h2]` ends at the next `[h2]` or `[h1]`, not at `[h3]`, which is a part of it),
///  - a block of code written in many lines: `[cpp]`, `[code]`, `[py]` or `[log]` up to its closing tag,
///  - a `[div]` (with or without attributes, e.g. `[div class="tip"]`) written in many lines, up to its closing tag.
/// The content of the blocks of code is verbatim, so a `[h1]` or `[cpp]` written inside of a program starts nothing.
/// The tags are read with StcTagScanner, like everywhere else.
namespace stcfold
{
enum class Kind
{
    Header, ///< `[h1]`...`[h6]`: the section of the header
    Code,   ///< `[cpp]`, `[code]`, `[py]`, `[log]` written in many lines
    Div,    ///< `[div]` written in many lines
};

struct Region
{
    int startLine = 0; ///< zero-based line with the header / the opening tag; it stays visible when the region is folded
    int endLine = 0;   ///< zero-based last line which is hidden when the region is folded (inclusive); always > startLine
    Kind kind = Kind::Header;
    int level = 0;     ///< 1..6 for a header, 0 for the blocks
    QString tag;       ///< lowercase name of the tag: "h2", "cpp"...
    /// Last line which belongs to the region for the user: for a block it is the line of the closing tag, which stays
    /// visible when the block is folded (so `endLine + 1` in the usual case), for a header it is `endLine`.
    int extentEndLine = 0;

    int hiddenLinesCount() const { return endLine - startLine; }
    bool hides(int line) const { return line > startLine && line <= endLine; }
    /// The line is the first line, is hidden by the region, or is the closing tag of the block of code
    bool encloses(int line) const { return line >= startLine && line <= extentEndLine; }

    bool operator==(const Region&) const = default;
};

/// The regions of the document given as its lines (without the line breaks), sorted by `startLine`.
/// A region with nothing to hide (`[cpp]x[/cpp]`, a header at the end of the document) is not foldable, so it is not here.
/// When regions start in the same line, the header goes first, then the bigger one.
QVector<Region> findRegions(const QStringList& lines);
} // namespace stcfold
