#include <vector>
#include <QSet>
#include <QStringList>

#include "DiffCalculation.h"

#include "pydifflib-cpp/difflib.hpp" /// it uses https://github.com/dominicprice/pydifflib-cpp

#include "diff-match-patch-cpp-stl/diff_match_patch.h" /// it uses https://github.com/leutloff/diff-match-patch-cpp-stl/

QSet<int> DiffCalculation::calculateModifiedLines(const QStringList& oldLines, const QStringList& newLines)
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

std::vector<DiffCalculation::DiffLine> DiffCalculation::computeDiff(const QStringList &oldLines, const QStringList &newLines)
{
    using namespace DiffCalculation;
    using namespace pydifflib;

    std::vector<std::string> a, b;
    for (const auto &line : oldLines)
        a.push_back(line.toStdString());
    for (const auto &line : newLines)
        b.push_back(line.toStdString());

    SequenceMatcher matcher(a, b);
    auto opcodes = matcher.get_opcodes();

    std::vector<DiffLine> result;

    for (const auto &op : opcodes)
    {
        int i1 = op.i1, i2 = op.i2; // old
        int j1 = op.j1, j2 = op.j2; // new

        switch (op.tag)
        {
        case tag_t::t_equal:
            for (int k = 0; k < i2 - i1; ++k)
            {
                result.push_back(DiffLine{
                    .oldIndex = i1 + k,
                    .newIndex = j1 + k,
                    .oldText = QString::fromStdString(a[i1 + k]),
                    .newText = QString::fromStdString(b[j1 + k]),
                    .type = DiffType::Unchanged
                });
            }
            break;

        case tag_t::t_replace:
        {
            int len = std::max(i2 - i1, j2 - j1);
            for (int k = 0; k < len; ++k)
            {
                int oldIdx = i1 + k;
                int newIdx = j1 + k;

                bool hasOld = oldIdx < i2;
                bool hasNew = newIdx < j2;

                QString oldText = hasOld ? QString::fromStdString(a[oldIdx]) : "";
                QString newText = hasNew ? QString::fromStdString(b[newIdx]) : "";

                DiffType type;
                if (hasOld && hasNew) {
                    type = (oldText == newText) ? DiffType::Unchanged : DiffType::Modified;
                } else if (hasOld) {
                    type = DiffType::Removed;
                } else {
                    type = DiffType::Added;
                }

                result.push_back(DiffLine{
                    .oldIndex = hasOld ? oldIdx : -1,
                    .newIndex = hasNew ? newIdx : -1,
                    .oldText = oldText,
                    .newText = newText,
                    .type = type
                });
            }
        }
        break;

        case tag_t::t_delete:
            for (int k = i1; k < i2; ++k)
            {
                result.push_back(DiffLine{
                    .oldIndex = k,
                    .newIndex = -1,
                    .oldText = QString::fromStdString(a[k]),
                    .newText = "",
                    .type = DiffType::Removed
                });
            }
            break;

        case tag_t::t_insert:
            for (int k = j1; k < j2; ++k)
            {
                result.push_back(DiffLine{
                    .oldIndex = -1,
                    .newIndex = k,
                    .oldText = "",
                    .newText = QString::fromStdString(b[k]),
                    .type = DiffType::Added
                });
            }
            break;
        }
    }

    return result;
}

///////////////////////////////////////////////
template <>
struct diff_match_patch_traits<char32_t>
{
    static bool is_alnum(char32_t c) { return std::isalnum(static_cast<unsigned int>(c)); }
    static bool is_digit(char32_t c) { return std::isdigit(static_cast<unsigned int>(c)); }
    static bool is_space(char32_t c) { return std::isspace(static_cast<unsigned int>(c)); }

    static int to_int(const char32_t* s)
    {
        QString str = QString::fromUcs4(reinterpret_cast<const char32_t*>(s));
        bool ok = false;
        int val = str.toInt(&ok);
        return ok ? val : 0;
    }

    static char32_t to_wchar(char32_t c) { return c; }
    static char32_t from_wchar(wchar_t c) { return static_cast<char32_t>(c); }

    static const char32_t* cs(const char32_t* s) { return s; }

    static const char32_t* cs(const wchar_t* s)
    {
        static thread_local std::u32string buffer;
        buffer = QString::fromWCharArray(s).toStdU32String();
        return buffer.c_str();
    }

    static constexpr char32_t eol = U'\n';
    static constexpr char32_t tab = U'\t';
};

namespace DiffCalculation {
QList<LineDiffResult> computeModifiedLineDiffs(const std::vector<DiffLine>& diffLines)
{
    //using namespace DiffCalculation;
    using DMP = diff_match_patch<std::u32string>;
    using Op = DMP::Operation;

    DMP dmp;
    QList<LineDiffResult> results;

    for (const DiffLine &line : diffLines)
    {
        QList<LineDiffFragment> oldFragments;
        QList<LineDiffFragment> newFragments;

        switch (line.type)
        {
        case DiffType::Modified:
        {
            std::u32string oldText = line.oldText.toStdU32String();
            std::u32string newText = line.newText.toStdU32String();

            auto diffs = dmp.diff_main(oldText, newText);
            dmp.diff_cleanupSemantic(diffs);

            for (const auto &d : diffs)
            {
                QString text = QString::fromUcs4(d.text.data(), static_cast<int>(d.text.size()));

                switch (d.operation)
                {
                case Op::EQUAL:
                    oldFragments.append({ FragmentType::Equal, text });
                    newFragments.append({ FragmentType::Equal, text });
                    break;
                case Op::DELETE:
                    oldFragments.append({ FragmentType::Delete, text });
                    break;
                case Op::INSERT:
                    newFragments.append({ FragmentType::Insert, text });
                    break;
                }
            }
            break;
        }

        case DiffType::Added:
            newFragments.append({ FragmentType::Insert, line.newText });
            break;

        case DiffType::Removed:
            oldFragments.append({ FragmentType::Delete, line.oldText });
            break;

        default:
            // Unchanged shouldn't be in this list, but if it is, skip
            continue;
        }

        results.append(LineDiffResult{
            .oldLineIndex = line.oldIndex,
            .newLineIndex = line.newIndex,
            .oldFragments = std::move(oldFragments),
            .newFragments = std::move(newFragments)
        });
    }

    return results;
}

QList<LineDiffResult> computeAllLineDiffs(const std::vector<DiffLine>& diffLines)
{
    using namespace DiffCalculation;
    using DMP = diff_match_patch<std::u32string>;
    using Op = DMP::Operation;

    DMP dmp;
    QList<LineDiffResult> results;

    for (const DiffLine &line : diffLines)
    {
        QList<LineDiffFragment> oldFragments;
        QList<LineDiffFragment> newFragments;

        switch (line.type)
        {
        case DiffType::Modified:
        {
            std::u32string oldText = line.oldText.toStdU32String();
            std::u32string newText = line.newText.toStdU32String();

            auto diffs = dmp.diff_main(oldText, newText);
            dmp.diff_cleanupSemantic(diffs);

            for (const auto &d : diffs)
            {
                QString text = QString::fromUcs4(d.text.data(), static_cast<int>(d.text.size()));

                switch (d.operation)
                {
                case Op::EQUAL:
                    oldFragments.append({ FragmentType::Equal, text });
                    newFragments.append({ FragmentType::Equal, text });
                    break;
                case Op::DELETE:
                    oldFragments.append({ FragmentType::Delete, text });
                    break;
                case Op::INSERT:
                    newFragments.append({ FragmentType::Insert, text });
                    break;
                }
            }
            break;
        }

        case DiffType::Added:
            newFragments.append({ FragmentType::Insert, line.newText });
            break;

        case DiffType::Removed:
            oldFragments.append({ FragmentType::Delete, line.oldText });
            break;

        case DiffType::Unchanged:
            oldFragments.append({ FragmentType::Equal, line.oldText });
            newFragments.append({ FragmentType::Equal, line.newText });
            break;
        }

        results.append(LineDiffResult{
            .oldLineIndex = line.oldIndex,
            .newLineIndex = line.newIndex,
            .oldFragments = std::move(oldFragments),
            .newFragments = std::move(newFragments)
        });
    }

    return results;
}
} // namespace DiffCalculation
