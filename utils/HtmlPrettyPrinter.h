#pragma once

#include <QString>

/// Making the HTML which cpp0x.pl returns readable: it is one long line, with all the tags next to each other.
namespace HtmlPrettyPrinter
{
/// The same HTML in many lines, indented by the nesting of the block elements (`div`, `table`, `tr`, `td`, `ul`, `li`, `h1`...).
///
/// - the inline elements (`span`, `b`, `a`...) and the text stay in the line they are in (a highlighted program has
///   a `span` for every word - a line for each of them would be unreadable),
/// - `<br>` ends a line, so a block of code is shown line by line, as it was written,
/// - the content of `pre`, `script`, `style` and `textarea` is not touched,
/// - runs of whitespace outside of them are one space - as they are for a browser, so the page looks the same.
///
/// Only the layout of the text is changed: the tags, the attributes and the text are the same, so it also is
/// a valid input of the function (and it gives the same result).
QString prettyPrint(const QString& html);
} // namespace HtmlPrettyPrinter
