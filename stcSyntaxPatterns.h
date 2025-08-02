#pragma once
#include <QRegularExpression>

namespace stc
{
namespace syntax
{
    extern const QRegularExpression stdDefaultTagRe;
    // DIV
    extern const QRegularExpression divOpenRe;
    extern const QRegularExpression divCloseRe;
    // PKT / CSV
    extern const QRegularExpression pktOpenRe;
    extern const QRegularExpression pktCloseRe;
    extern const QRegularExpression csvOpenRe;
    extern const QRegularExpression csvCloseRe;
    extern const QRegularExpression runTagRe;
    // CODE BLOCKS
    extern const QRegularExpression codeBlockOpenRe;
    extern const QRegularExpression codeCloseRe;
    extern const QRegularExpression cppCloseRe;
    extern const QRegularExpression pythonCloseRe;
    // href and image tags with attributes:
    extern const QRegularExpression anchorRe;
    extern const QRegularExpression imgRe;
    extern const QRegularExpression imgAttributeSrcRe;
    extern const QRegularExpression imgAttributeAltRe;
    extern const QRegularExpression imgAttributeDescRe;
    extern const QRegularExpression imgAttributeAutofitRe;
    // others
    extern const QRegularExpression baseFormatting_boldItalicUnderlineStrikeRe;
    extern const QRegularExpression boldCloseRe;
    extern const QRegularExpression italicCloseRe;
    extern const QRegularExpression underlineCloseRe;
    extern const QRegularExpression strikeOutCloseRe;

    extern const QRegularExpression wordWithPolishCharactersRe;
} // namespace syntax
} // namespace stc
