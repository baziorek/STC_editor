#include <stddef.h>
#include <algorithm>
#include <array>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include <QSet>
#include <QStringList>
#include <QChar>
#include <QStringView>
#include <QtGlobal>
#include <cctype>
#include <utility>

#include "DiffCalculation.h"
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
/// Timeout (seconds) for the whole-document line diff; on timeout diff_match_patch falls back
/// to a coarse delete+insert for the part it could not resolve in time.
constexpr float lineDiffTimeoutSeconds = 2.0f;
/// Timeout (seconds) for character-level diffs and line similarity estimates.
constexpr float charDiffTimeoutSeconds = 0.25f;

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

/// "Fancy replace" of difflib's Differ, reimplemented on top of diff_match_patch: within a block
/// of removed/added lines the most similar pair (similarity above 0.74, i.e. difflib's cutoff of
/// 0.75) becomes an anchor shown as modified, and the lines before and after it are handled
/// recursively. Lines without a similar counterpart stay unpaired (removed / added); a run of
/// such lines is then shown side by side by position.
struct ReplaceDiffer
{
    const std::vector<std::string> &a;
    const std::vector<std::string> &b;
    std::vector<DiffLine> &result;
    std::vector<int> pendingOld;
    std::vector<int> pendingNew;
    diff_match_patch<std::string> similarityDmp;

    ReplaceDiffer(const std::vector<std::string> &a, const std::vector<std::string> &b, std::vector<DiffLine> &result)
        : a(a), b(b), result(result)
    {
        similarityDmp.Diff_Timeout = charDiffTimeoutSeconds;
    }

    void flushPending()
    {
        pairByPosition(a, b, pendingOld, pendingNew, result);
        pendingOld.clear();
        pendingNew.clear();
    }

    void emitPair(int i, int j, DiffType type)
    {
        flushPending();
        result.push_back(makeDiffLine(a, b, i, j, type));
    }

    void dumpOld(int i1, int i2)
    {
        for (int i = i1; i < i2; ++i) pendingOld.push_back(i);
    }

    void dumpNew(int j1, int j2)
    {
        for (int j = j1; j < j2; ++j) pendingNew.push_back(j);
    }

    void plainReplace(int i1, int i2, int j1, int j2)
    {
        if (j2 - j1 < i2 - i1) { dumpNew(j1, j2); dumpOld(i1, i2); }
        else                   { dumpOld(i1, i2); dumpNew(j1, j2); }
    }

    /// Similarity of two lines in [0,1], like SequenceMatcher::ratio() (2*matches/(len1+len2)),
    /// computed from the character diff. The cheap upper bounds are checked first, like difflib.
    double similarity(const std::string &x, const std::string &y, double threshold) const
    {
        const double total = static_cast<double>(x.size() + y.size());
        if (total == 0.0)
            return 1.0;

        const double realQuick = 2.0 * std::min(x.size(), y.size()) / total;
        if (realQuick <= threshold)
            return realQuick;

        std::array<int, 256> countX{}, countY{};
        for (const char c : x) ++countX[static_cast<unsigned char>(c)];
        for (const char c : y) ++countY[static_cast<unsigned char>(c)];
        size_t common = 0;
        for (size_t k = 0; k < countX.size(); ++k)
            common += static_cast<size_t>(std::min(countX[k], countY[k]));
        const double quick = 2.0 * common / total;
        if (quick <= threshold)
            return quick;

        size_t equal = 0;
        for (const auto &d : similarityDmp.diff_main(x, y, false))
            if (d.operation == diff_match_patch<std::string>::EQUAL)
                equal += d.text.size();
        return 2.0 * equal / total;
    }

    void fancyReplace(int i1, int i2, int j1, int j2)
    {
        double bestRatio = 0.74;
        int bestI = i1, bestJ = j1;
        bool eqFound = false;
        int eqI = 0, eqJ = 0;

        for (int j = j1; j < j2; ++j)
        {
            for (int i = i1; i < i2; ++i)
            {
                if (a[i] == b[j])
                {
                    if (!eqFound) { eqFound = true; eqI = i; eqJ = j; }
                    continue;
                }
                const double ratio = similarity(a[i], b[j], bestRatio);
                if (ratio > bestRatio)
                {
                    bestRatio = ratio;
                    bestI = i;
                    bestJ = j;
                }
            }
        }

        if (bestRatio < 0.75)
        {
            if (!eqFound)
            {
                plainReplace(i1, i2, j1, j2);
                return;
            }
            bestI = eqI;
            bestJ = eqJ;
        }
        else
        {
            eqFound = false;
        }

        fancyHelper(i1, bestI, j1, bestJ);
        emitPair(bestI, bestJ, eqFound ? DiffType::Unchanged : DiffType::Modified);
        fancyHelper(bestI + 1, i2, bestJ + 1, j2);
    }

    void fancyHelper(int i1, int i2, int j1, int j2)
    {
        if (i1 < i2)
        {
            if (j1 < j2) fancyReplace(i1, i2, j1, j2);
            else         dumpOld(i1, i2);
        }
        else if (j1 < j2)
        {
            dumpNew(j1, j2);
        }
    }
};

/// Handles a block [i1,i2) of old lines that has to become [j1,j2) of new lines.
///
/// Pairing k-th with k-th is wrong as soon as a line is inserted or removed inside the block - all
/// the following pairs are then shifted. So the lines are matched by similarity (see ReplaceDiffer);
/// lines that have no similar counterpart stay unpaired (removed / added); a run of such lines is
/// then shown side by side by position.
void appendReplaceBlock(const std::vector<std::string> &a, const std::vector<std::string> &b,
                        int i1, int i2, int j1, int j2, std::vector<DiffLine> &result)
{
    unsigned long long oldBytes = 0, newBytes = 0;
    for (int i = i1; i < i2; ++i) oldBytes += a[i].size() + 1;
    for (int j = j1; j < j2; ++j) newBytes += b[j].size() + 1;

    if (oldBytes * newBytes > maxSimilarityWork)
    {
        std::vector<int> oldIndexes, newIndexes;
        for (int i = i1; i < i2; ++i) oldIndexes.push_back(i);
        for (int j = j1; j < j2; ++j) newIndexes.push_back(j);
        pairByPosition(a, b, oldIndexes, newIndexes, result);
        return;
    }

    ReplaceDiffer differ(a, b, result);
    differ.fancyReplace(i1, i2, j1, j2);
    differ.flushPending();
}

enum class LineOpTag
{
    Equal,
    Delete,
    Insert,
    Replace
};

struct LineOp
{
    LineOpTag tag;
    int i1, i2, j1, j2;
};

/// Line-level diff produced by diff_match_patch: each distinct line is encoded as a single
/// char32_t token and the token strings are diffed (checklines=false - the built-in line mode
/// expects '\n'-separated text, which the token strings do not contain). Runs of deletes and
/// inserts become "replace" blocks, like the opcodes of difflib's SequenceMatcher.
std::vector<LineOp> computeLineOps(const std::vector<std::string> &a, const std::vector<std::string> &b)
{
    std::unordered_map<std::string, char32_t> ids;
    ids.reserve(a.size() + b.size());
    std::u32string textA, textB;
    textA.reserve(a.size());
    textB.reserve(b.size());

    auto encode = [&ids](const std::vector<std::string> &lines, std::u32string &out) {
        for (const auto &line : lines)
        {
            auto it = ids.find(line);
            if (it == ids.end())
                it = ids.emplace(line, static_cast<char32_t>(ids.size() + 1)).first;
            out.push_back(it->second);
        }
    };
    encode(a, textA);
    encode(b, textB);

    using Dmp = diff_match_patch<std::u32string>;
    Dmp dmp;
    dmp.Diff_Timeout = lineDiffTimeoutSeconds;
    const auto diffs = dmp.diff_main(textA, textB, false);

    std::vector<LineOp> ops;
    size_t i = 0, j = 0;
    for (auto it = diffs.begin(); it != diffs.end(); )
    {
        const int length = static_cast<int>(it->text.size());
        if (it->operation == Dmp::EQUAL)
        {
            ops.push_back({ LineOpTag::Equal, static_cast<int>(i), static_cast<int>(i) + length,
                            static_cast<int>(j), static_cast<int>(j) + length });
            i += length;
            j += length;
            ++it;
            continue;
        }

        int deleted = 0, inserted = 0;
        while (it != diffs.end() && it->operation != Dmp::EQUAL)
        {
            const int l = static_cast<int>(it->text.size());
            if (it->operation == Dmp::DELETE) deleted += l; else inserted += l;
            ++it;
        }

        if (deleted && inserted)
            ops.push_back({ LineOpTag::Replace, static_cast<int>(i), static_cast<int>(i) + deleted,
                            static_cast<int>(j), static_cast<int>(j) + inserted });
        else if (deleted)
            ops.push_back({ LineOpTag::Delete, static_cast<int>(i), static_cast<int>(i) + deleted,
                            static_cast<int>(j), static_cast<int>(j) });
        else
            ops.push_back({ LineOpTag::Insert, static_cast<int>(i), static_cast<int>(i),
                            static_cast<int>(j), static_cast<int>(j) + inserted });
        i += deleted;
        j += inserted;
    }
    return ops;
}
} // namespace

QSet<int> calculateModifiedLines(const QStringList& oldLines, const QStringList& newLines)
{
    std::vector<std::string> a, b;
    for (const auto& line : oldLines)
        a.emplace_back(line.toStdString());
    for (const auto& line : newLines)
        b.emplace_back(line.toStdString());

    QSet<int> modified;

    for (const auto &op : computeLineOps(a, b))
    {
        if (op.tag != LineOpTag::Equal)
        {
            for (int k = op.j1; k < op.j2; ++k)
                modified.insert(k + 1);  // linie liczymy od 1
        }
    }

    return modified;
}

std::vector<DiffLine> computeDiff(const QStringList &oldLines, const QStringList &newLines)
{
    std::vector<std::string> a, b;
    for (const auto &line : oldLines)
        a.push_back(line.toStdString());
    for (const auto &line : newLines)
        b.push_back(line.toStdString());

    std::vector<DiffLine> result;

    for (const auto &op : computeLineOps(a, b))
    {
        int i1 = op.i1, i2 = op.i2; // old
        int j1 = op.j1, j2 = op.j2; // new

        switch (op.tag)
        {
        case LineOpTag::Equal:
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

        case LineOpTag::Replace:
            appendReplaceBlock(a, b, i1, i2, j1, j2, result);
            break;

        case LineOpTag::Delete:
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

        case LineOpTag::Insert:
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
