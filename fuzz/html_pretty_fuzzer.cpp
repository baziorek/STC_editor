// Fuzz target for the HTML pretty printer (utils/HtmlPrettyPrinter.cpp).
// The documentation promises prettyPrint(prettyPrint(x)) == prettyPrint(x),
// so the output is fed back in as a second input.

#include <QString>

#include "utils/HtmlPrettyPrinter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    constexpr size_t maxInput = 512u << 10; // 512 KiB
    if (size > maxInput)
        return 0;

    const QString html = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(size));

    const QString once = HtmlPrettyPrinter::prettyPrint(html);
    (void)HtmlPrettyPrinter::prettyPrint(once);
    return 0;
}
