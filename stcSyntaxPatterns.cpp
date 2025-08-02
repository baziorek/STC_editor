#include "stcSyntaxPatterns.h"

namespace stc
{
namespace syntax
{
    const QRegularExpression stdDefaultTagRe(R"(\[/?\w+(=[^\]]+)?\])");
    // DIV
    const QRegularExpression divOpenRe(R"___(\[(div)(?:\s+class="(tip|uwaga)")?\]|\[(cytat)\])___");
    const QRegularExpression divCloseRe(R"___(\[/div\]|\[/cytat\])___");
    // PKT / CSV
    const QRegularExpression pktOpenRe(R"(\[pkt(\s+[^\]]*)?\])");
    const QRegularExpression pktCloseRe(R"(\[/pkt\])");
    const QRegularExpression csvOpenRe(R"(\[csv(\s+[^\]]*)?\])");
    const QRegularExpression csvCloseRe(R"(\[/csv\])");
    const QRegularExpression runTagRe(R"(\[run\](.*?)\[/run\])");
    // CODE BLOCKS
    const QRegularExpression codeBlockOpenRe(R"__(\[(cpp|py|code)(?:\s+src="([^"]+)")?\])__");
    const QRegularExpression codeCloseRe(R"(\[/code\])");
    const QRegularExpression cppCloseRe(R"(\[/cpp\])");
    const QRegularExpression pythonCloseRe(R"(\[/py\])");
    // href and image tags with attributes:
    const QRegularExpression anchorRe(R"__(\[a\s+href="([^"]+)"(?:\s+name="([^"]+)")?\])__"); // Match [a href="..."] or [a href="..." name="..."]
    const QRegularExpression imgRe(R"__(
        \[img\s+
        (?:
            (?:src="([^"]+)")|
            (?:alt="([^"]*)")|
            (?:opis="([^"]*)")|
            (?:autofit\b)
        )
        (?:\s+
            (?:
                (?:src="([^"]+)")|
                (?:alt="([^"]*)")|
                (?:opis="([^"]*)")|
                (?:autofit\b)
            )
        )*
        \s*\])__", QRegularExpression::ExtendedPatternSyntaxOption); // Match [img ...] with optional attributes in any order
    const QRegularExpression imgAttributeSrcRe(R"__(src="([^"]+)")__");
    const QRegularExpression imgAttributeAltRe(R"__(alt="([^"]*)")__");
    const QRegularExpression imgAttributeDescRe(R"__(opis="([^"]*)")__");
    const QRegularExpression imgAttributeAutofitRe(R"__(\bautofit\b)__");
    // others
    const QRegularExpression baseFormatting_boldItalicUnderlineStrikeRe(R"(\[(b|i|u|s)\])");
    const QRegularExpression boldCloseRe(R"(\[/b\])");
    const QRegularExpression italicCloseRe(R"(\[/i\])");
    const QRegularExpression underlineCloseRe(R"(\[/u\])");
    const QRegularExpression strikeOutCloseRe(R"(\[/s\])");

    // const QRegularExpression wordWithPolishCharactersRe(R"(\b\p{L}+(?:[-']\p{L}+)*\b)"); - it does not catch polish
    const QRegularExpression wordWithPolishCharactersRe(R"([A-Za-zĄĆĘŁŃÓŚŹŻąćęłńóśźż]{2,})");
} // namespace syntax
} // namespace stc
