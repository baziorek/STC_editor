#include <algorithm>
#include <string>
#include <vector>
#include <QSet>
#include <QStringList>

#include "DiffCalculation.h"

#include "pydifflib-cpp/difflib.hpp" /// it uses https://github.com/dominicprice/pydifflib-cpp

#include "diff-match-patch-cpp-stl/diff_match_patch.h" /// it uses https://github.com/leutloff/diff-match-patch-cpp-stl/


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


namespace DiffCalculation
{
namespace
{
/// Looking for similar line pairs costs about (bytes of old lines) x (bytes of new lines) in a changed block
/// (about 2.5 s for 200 x 200 unrelated lines of 500 bytes, 10x more without optimizations). Above this limit the block is paired by position instead.
constexpr unsigned long long maxSimilarityWork = 400'000'000ULL;

DiffLine makeDiffLine(const std::vector<std::string> &a, const std::vector<std::string> &b, int oldIndex, int newIndex, DiffType type)
{
    return DiffLine{
        .oldIndex = oldIndex,
        .newIndex = newIndex,
        .oldText = oldIndex >= 0 ? QString::fromStdString(a[oldIndex]) : QString(),
        .newText = newIndex >= 0 ? QString::fromStdString(b[newIndex]) : QString(),
        .type = type
    };
}

/// Pairs up the k-th removed line with the k-th added line (side by side, like a changed chunk in meld);
/// what is left over stays removed / added.
void pairByPosition(const std::vector<std::string> &a, const std::vector<std::string> &b,
                    const std::vector<int> &oldIndexes, const std::vector<int> &newIndexes,
                    std::vector<DiffLine> &result)
{
    const size_t common = std::min(oldIndexes.size(), newIndexes.size());

    for (size_t k = 0; k < common; ++k)
    {
        const bool same = a[oldIndexes[k]] == b[newIndexes[k]];
        result.push_back(makeDiffLine(a, b, oldIndexes[k], newIndexes[k], same ? DiffType::Unchanged : DiffType::Modified));
    }
    for (size_t k = common; k < oldIndexes.size(); ++k)
        result.push_back(makeDiffLine(a, b, oldIndexes[k], -1, DiffType::Removed));
    for (size_t k = common; k < newIndexes.size(); ++k)
        result.push_back(makeDiffLine(a, b, -1, newIndexes[k], DiffType::Added));
}

/// Handles a block [i1,i2) of old lines that has to become [j1,j2) of new lines (difflib's "replace").
///
/// Pairing k-th with k-th is wrong as soon as a line is inserted or removed inside the block - all
/// the following pairs are then shifted. So the lines are matched by similarity, using the library's
/// Differ (the algorithm of Python's difflib.ndiff): the most similar pair is an anchor, the lines before
/// and after the anchor are handled recursively. Lines that have no similar counterpart stay unpaired
/// (removed / added); a run of such lines is then shown side by side by position.
void appendReplaceBlock(const std::vector<std::string> &a, const std::vector<std::string> &b,
                        int i1, int i2, int j1, int j2, std::vector<DiffLine> &result)
{
    std::vector<int> pendingOld, pendingNew;

    auto flushPending = [&]() {
        pairByPosition(a, b, pendingOld, pendingNew, result);
        pendingOld.clear();
        pendingNew.clear();
    };

    unsigned long long oldBytes = 0, newBytes = 0;
    for (int i = i1; i < i2; ++i) oldBytes += a[i].size() + 1;
    for (int j = j1; j < j2; ++j) newBytes += b[j].size() + 1;

    if (oldBytes * newBytes > maxSimilarityWork)
    {
        for (int i = i1; i < i2; ++i) pendingOld.push_back(i);
        for (int j = j1; j < j2; ++j) pendingNew.push_back(j);
        flushPending();
        return;
    }

    const std::vector<std::string> oldBlock(a.begin() + i1, a.begin() + i2);
    const std::vector<std::string> newBlock(b.begin() + j1, b.begin() + j2);

    pydifflib::Differ<std::string> differ;
    int oldIndex = i1, newIndex = j1;

    for (const auto &delta : differ.get_deltas(oldBlock, newBlock))
    {
        using pydifflib::tag_t;

        switch (delta.tag)
        {
        case tag_t::t_delete:
            pendingOld.push_back(oldIndex++);
            break;
        case tag_t::t_insert:
            pendingNew.push_back(newIndex++);
            break;
        case tag_t::t_replace: // similar lines: a pair
            flushPending();
            result.push_back(makeDiffLine(a, b, oldIndex++, newIndex++, DiffType::Modified));
            break;
        default: // t_equal: identical line found inside the block
            flushPending();
            result.push_back(makeDiffLine(a, b, oldIndex++, newIndex++, DiffType::Unchanged));
            break;
        }
    }
    flushPending();
}
} // namespace

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

std::vector<DiffLine> computeDiff(const QStringList &oldLines, const QStringList &newLines)
{
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
            appendReplaceBlock(a, b, i1, i2, j1, j2, result);
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

QList<LineDiffResult> computeModifiedLineDiffs(const std::vector<DiffLine>& diffLines)
{
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

namespace
{
/// Above this many characters (of the part that differs) a text is not compared character by character
constexpr qsizetype maxCharDiffLength = 200'000;
constexpr float charDiffTimeoutSeconds = 0.25f;

struct CommonEnds
{
    qsizetype prefix = 0;
    qsizetype suffix = 0;
};

/// Length of the text equal at the start and at the end of both strings (never splitting a surrogate pair)
CommonEnds commonEnds(const QString& a, const QString& b)
{
    CommonEnds ends;
    const qsizetype shortest = std::min(a.size(), b.size());

    while (ends.prefix < shortest && a[ends.prefix] == b[ends.prefix])
        ++ends.prefix;
    while (ends.prefix > 0 && a[ends.prefix - 1].isHighSurrogate())
        --ends.prefix;

    while (ends.suffix < shortest - ends.prefix && a[a.size() - 1 - ends.suffix] == b[b.size() - 1 - ends.suffix])
        ++ends.suffix;
    while (ends.suffix > 0 && a[a.size() - ends.suffix].isLowSurrogate())
        --ends.suffix;

    return ends;
}

int codePoints(QStringView text)
{
    int count = 0;
    for (qsizetype i = 0; i < text.size(); ++i)
    {
        if (text[i].isHighSurrogate() && i + 1 < text.size() && text[i + 1].isLowSurrogate())
            ++i;
        ++count;
    }
    return count;
}
} // namespace

CharChangeCounts countCharChanges(const QString& oldText, const QString& newText)
{
    const CommonEnds ends = commonEnds(oldText, newText);
    const QString oldMiddle = oldText.mid(ends.prefix, oldText.size() - ends.prefix - ends.suffix);
    const QString newMiddle = newText.mid(ends.prefix, newText.size() - ends.prefix - ends.suffix);

    CharChangeCounts counts;
    if (oldMiddle.isEmpty() || newMiddle.isEmpty() || oldMiddle.size() + newMiddle.size() > maxCharDiffLength)
    {
        counts.removed = codePoints(oldMiddle);
        counts.inserted = codePoints(newMiddle);
        counts.approximate = !oldMiddle.isEmpty() && !newMiddle.isEmpty(); // a pure insertion / deletion is exact
        return counts;
    }

    using DMP = diff_match_patch<std::u32string>;
    DMP dmp;
    dmp.Diff_Timeout = charDiffTimeoutSeconds;
    const auto diffs = dmp.diff_main(oldMiddle.toStdU32String(), newMiddle.toStdU32String());
    for (const auto& d : diffs)
    {
        if (d.operation == DMP::Operation::INSERT)
            counts.inserted += static_cast<int>(d.text.size());
        else if (d.operation == DMP::Operation::DELETE)
            counts.removed += static_cast<int>(d.text.size());
    }
    return counts;
}

QList<LineDiffFragment> computeInlineDiff(const QString& oldText, const QString& newText)
{
    QList<LineDiffFragment> fragments;
    auto append = [&fragments](FragmentType type, const QString& text) {
        if (text.isEmpty())
            return;
        if (!fragments.isEmpty() && fragments.last().type == type)
            fragments.last().text += text;
        else
            fragments.append({ type, text });
    };

    const CommonEnds ends = commonEnds(oldText, newText);
    const QString oldMiddle = oldText.mid(ends.prefix, oldText.size() - ends.prefix - ends.suffix);
    const QString newMiddle = newText.mid(ends.prefix, newText.size() - ends.prefix - ends.suffix);

    append(FragmentType::Equal, oldText.left(ends.prefix));

    if (oldMiddle.isEmpty() || newMiddle.isEmpty() || oldMiddle.size() + newMiddle.size() > maxCharDiffLength)
    {
        append(FragmentType::Delete, oldMiddle);
        append(FragmentType::Insert, newMiddle);
    }
    else
    {
        using DMP = diff_match_patch<std::u32string>;
        DMP dmp;
        dmp.Diff_Timeout = charDiffTimeoutSeconds;
        auto diffs = dmp.diff_main(oldMiddle.toStdU32String(), newMiddle.toStdU32String());
        dmp.diff_cleanupSemantic(diffs);
        for (const auto& d : diffs)
        {
            const QString text = QString::fromUcs4(d.text.data(), static_cast<int>(d.text.size()));
            switch (d.operation)
            {
            case DMP::Operation::EQUAL:  append(FragmentType::Equal, text);  break;
            case DMP::Operation::DELETE: append(FragmentType::Delete, text); break;
            case DMP::Operation::INSERT: append(FragmentType::Insert, text); break;
            }
        }
    }

    append(FragmentType::Equal, oldText.right(ends.suffix));
    return fragments;
}

QList<LineDiffResult> computeAllLineDiffs(const std::vector<DiffLine>& diffLines)
{
    // using namespace DiffCalculation;
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
