#include "stcsyntaxpatterns.h"

namespace stc {
namespace syntax {
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
    const QRegularExpression anchorRe(R"__(\[a\s+href=\"([^\"]+)\"(?:\s+name=\"([^\"]+)\")?\])__");
    const QRegularExpression imgRe(R"__(\[img\s+(?: (?:src=\"([^\"]+)\")| (?:alt=\"([^\"]*)\")| (?:opis=\"([^\"]*)\")| (?:autofit\b) )+\])__");
    const QRegularExpression srcRe(R"__(src=\"([^\"]+)\")__");
    const QRegularExpression altRe(R"__(alt=\"([^\"]*)\")__");
    const QRegularExpression opisRe(R"__(opis=\"([^\"]*)\")__");
    const QRegularExpression autofitRe(R"__(\bautofit\b)__");
} // namespace syntax
} // namespace stc
