#include "diffcalculation.h"
#include "libs/pydifflib-cpp/difflib.hpp"

#include <QSet>
#include <QStringList>

QSet<int> calculateModifiedLines(const QStringList& oldLines, const QStringList& newLines)
{
    using namespace pydifflib;

    std::vector<std::string> a, b;
    for (const auto& line : oldLines)
        a.emplace_back(line.toStdString());
    for (const auto& line : newLines)
        b.emplace_back(line.toStdString());

    SequenceMatcher matcher(a, b);
    auto opcodes = matcher.get_opcodes();

    QSet<int> modified;

    for (const auto& op : opcodes)
    {
        if (op.tag != tag_t::t_equal)
        {
            for (int i = op.j1; i < op.j2; ++i)
                modified.insert(i + 1);  // linie liczymy od 1
        }
    }

    return modified;
}
