// Fuzz target for the line/character diff engine (utils/DiffCalculation.cpp).
// Input format: `<old text>@@@<new text>` — "@@@" separates the two documents;
// without it the whole input is the old document and the new one is empty.

#include <QList>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>

#include "utils/DiffCalculation.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    constexpr size_t maxInput = 1u << 20; // 1 MiB is plenty for a diff
    if (size > maxInput)
        return 0;

    const std::string input(reinterpret_cast<const char*>(data), size);
    const auto sep = input.find("@@@");
    const std::string oldPart = sep == std::string::npos ? input : input.substr(0, sep);
    const std::string newPart = sep == std::string::npos ? std::string() : input.substr(sep + 3);

    const QString oldText = QString::fromUtf8(oldPart.data(), static_cast<int>(oldPart.size()));
    const QString newText = QString::fromUtf8(newPart.data(), static_cast<int>(newPart.size()));

    const QStringList oldLines = oldText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    const QStringList newLines = newText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    using namespace DiffCalculation;
    (void)calculateModifiedLines(oldLines, newLines);

    const std::vector<DiffLine> diff = computeDiff(oldLines, newLines);
    (void)computeModifiedLineDiffs(diff);
    (void)computeAllLineDiffs(diff);

    // character-level entry points on the raw texts
    (void)countCharChanges(oldText, newText);
    (void)computeInlineDiff(oldText, newText);
    return 0;
}
