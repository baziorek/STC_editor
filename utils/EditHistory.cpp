#include "EditHistory.h"

#include <QSet>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <numeric>

namespace
{
/// Between so many lines (on each side) the similarity based pairing is used; above, lines are paired by position
constexpr int maxLinesForSimilarityPairing = 3000;

template <typename T>
void replaceRange(QList<T>& list, int position, int count, const QList<T>& with)
{
    if (count == with.size())
    {
        for (int i = 0; i < count; ++i)
            list[position + i] = with[i];
        return;
    }

    if (with.size() <= 32)
    {
        list.erase(list.begin() + position, list.begin() + position + count);
        for (int i = 0; i < with.size(); ++i)
            list.insert(position + i, with[i]);
        return;
    }

    QList<T> result;
    result.reserve(list.size() - count + with.size());
    result += list.mid(0, position);
    result += with;
    result += list.mid(position + count);
    list = std::move(result);
}

/// `numbers` ascending: 3 4 5 9 -> 3-5, 9
QList<EditHistory::LineRange> toRanges(const QList<int>& numbers)
{
    QList<EditHistory::LineRange> ranges;
    for (const int number : numbers)
    {
        if (!ranges.isEmpty() && ranges.last().last + 1 == number)
            ranges.last().last = number;
        else
            ranges.append({ number, number });
    }
    return ranges;
}

int commonPrefixLength(const QList<QString>& a, const QList<QString>& b)
{
    int length = 0;
    while (length < a.size() && length < b.size() && a[length] == b[length])
        ++length;
    return length;
}

/// Equal lines at the end, not counting the first `prefix` lines of both
int commonSuffixLength(const QList<QString>& a, const QList<QString>& b, int prefix)
{
    int length = 0;
    while (length < a.size() - prefix && length < b.size() - prefix && a[a.size() - 1 - length] == b[b.size() - 1 - length])
        ++length;
    return length;
}

/// For every new line: the index of the old line it is, or -1 for a line which is new.
/// Small regions are matched by similarity (a line inserted among edited ones does not take the identity of its
/// neighbour); big ones, and a single line against many, simply one by one.
QList<int> matchMiddleLines(const QList<QString>& oldMiddle, const QList<QString>& newMiddle)
{
    QList<int> newToOld(newMiddle.size(), -1);
    const bool bySimilarity = oldMiddle.size() > 1 && newMiddle.size() > 1
                              && oldMiddle.size() <= maxLinesForSimilarityPairing && newMiddle.size() <= maxLinesForSimilarityPairing;
    if (!bySimilarity)
    {
        for (int j = 0; j < std::min(oldMiddle.size(), newMiddle.size()); ++j)
            newToOld[j] = j;
        return newToOld;
    }

    const auto diff = DiffCalculation::computeDiff(QStringList(oldMiddle), QStringList(newMiddle));
    for (const auto& line : diff)
    {
        if (line.oldIndex >= 0 && line.newIndex >= 0)
            newToOld[line.newIndex] = line.oldIndex;
    }
    return newToOld;
}

/// Decides which of the new lines ARE the old lines (so a line keeps its identity, and its history, when it is edited,
/// pushed down by a line inserted above, ...) and which ones are new.
///   - lines equal at the start and at the end of the region are the same lines,
///   - what is left in between is matched by matchMiddleLines(),
///   - old lines left without a partner are removed, new lines without a partner are added.
/// `touched` receives the ids of the lines that were modified, added or removed.
/// When `allocateIds` is false the added lines get -1 (they get their real id when undo/redo has finished).
void pairLines(const QList<QString>& oldLines, const QList<int>& oldIds, const QList<QString>& newLines,
               QList<int>& newIds, QList<int>& touched, int& nextId, bool allocateIds)
{
    const int oldCount = oldLines.size();
    const int newCount = newLines.size();
    const int prefix = commonPrefixLength(oldLines, newLines);
    const int suffix = commonSuffixLength(oldLines, newLines, prefix);

    newIds = QList<int>(newCount, -1);
    for (int i = 0; i < prefix; ++i)
        newIds[i] = oldIds[i];
    for (int i = 0; i < suffix; ++i)
        newIds[newCount - 1 - i] = oldIds[oldCount - 1 - i];

    const int oldMiddleCount = oldCount - prefix - suffix;
    const int newMiddleCount = newCount - prefix - suffix;
    if (oldMiddleCount == 0 && newMiddleCount == 0)
        return;

    const QList<int> newToOld = matchMiddleLines(oldLines.mid(prefix, oldMiddleCount), newLines.mid(prefix, newMiddleCount));
    QList<bool> oldKept(oldMiddleCount, false);
    for (int j = 0; j < newMiddleCount; ++j)
    {
        const int target = prefix + j;
        const int source = newToOld[j];
        if (source >= 0)
        {
            oldKept[source] = true;
            newIds[target] = oldIds[prefix + source];
            if (oldLines[prefix + source] != newLines[target])
                touched.append(newIds[target]);
        }
        else if (allocateIds)
        {
            newIds[target] = nextId++;
            touched.append(newIds[target]);
        }
    }

    for (int i = 0; i < oldMiddleCount; ++i)
    {
        if (!oldKept[i])
            touched.append(oldIds[prefix + i]);
    }
}
} // namespace


qint64 EditHistory::Step::characters() const
{
    qint64 total = 0;
    for (const QString& line : oldLines)
        total += line.size();
    for (const QString& line : newLines)
        total += line.size();
    return total;
}


EditHistory::UndoRedoScope::UndoRedoScope(EditHistory& history, Kind kind)
    : history(history), outermost(history.operation == Operation::Edit)
{
    if (!outermost)
        return;
    history.operation = (kind == Kind::Undo) ? Operation::Undo : Operation::Redo;
    history.operationEvents = 0;
}

EditHistory::UndoRedoScope::~UndoRedoScope()
{
    if (outermost)
        history.finishOperation();
}


EditHistory::EditHistory(QTextDocument* document, QObject* parent)
    : QObject(parent), document(document)
{
    Q_ASSERT(document);
    connect(document, &QTextDocument::contentsChange, this, &EditHistory::onContentsChange);
    connect(document, &QTextDocument::undoCommandAdded, this, &EditHistory::onUndoCommandAdded);
    resync("created");
}

EditHistory::~EditHistory() = default;

void EditHistory::undo()
{
    UndoRedoScope scope(*this, UndoRedoScope::Kind::Undo);
    document->undo();
}

void EditHistory::redo()
{
    UndoRedoScope scope(*this, UndoRedoScope::Kind::Redo);
    document->redo();
}

void EditHistory::reset()
{
    resync("reset");
}

void EditHistory::noteSaved()
{
    saves.append({ current, QDateTime::currentDateTime(), false });
    emit changed();
}

void EditHistory::setTextBudget(qint64 characters)
{
    budget = std::max<qint64>(characters, 0);
    enforceBudget();
}

void EditHistory::resync(const char* reason)
{
    ++restarts;
    restartReason = reason;
    lines.clear();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
        lines.append(block.text()); // not toPlainText(): that one turns U+00A0 into a space and U+2028 into a line break

    ids = QList<int>(lines.size());
    std::iota(ids.begin(), ids.end(), 0);
    nextId = static_cast<int>(lines.size());

    steps.clear();
    current = 0;
    touches.clear();
    saves.clear();
    newStepPending = false;
    operationEvents = 0;
    placeholderIds = false;
    retained = 0;
    baseline = QDateTime::currentDateTime();
    emit changed();
}

void EditHistory::onUndoCommandAdded()
{
    // Qt announces a new undo step BEFORE it reports the first change of that step
    if (operation == Operation::Edit)
        newStepPending = true;
}

void EditHistory::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    if (!document->isUndoRedoEnabled())
    {
        resync("content replaced"); // setPlainText(), clear(): Qt dropped its undo stack too
        return;
    }

    const bool recording = (operation == Operation::Edit);
    if (recording && !isOrdinaryEdit())
    {
        resync("not an edit we know of");
        return;
    }
    if (recording)
        dropRedoBranch();

    Region region;
    if (!computeRegion(position, charsRemoved, charsAdded, recording, region))
    {
        resync("lost track of the document"); // better to start over than to show nonsense
        return;
    }

    if (recording)
        recordRegion(region);
    else
        ++operationEvents;

    applyRegion(region);

    if (recording)
    {
        enforceBudget();
        emit changed();
    }
}

bool EditHistory::isOrdinaryEdit() const
{
    // Otherwise this was not an edit made in a way we know of: e.g. QTextDocument::clear(), or an undo which did not
    // go through UndoRedoScope.
    return document->availableUndoSteps() > 0 && document->availableRedoSteps() == 0;
}

void EditHistory::recordRegion(const Region& region)
{
    const bool startsNewStep = newStepPending || current == 0;
    newStepPending = false;
    if (startsNewStep)
        beginStep(region);
    else
        extendStep(steps[current - 1], region);
}

bool EditHistory::computeRegion(int position, int charsRemoved, int charsAdded, bool allocateIds, Region& region)
{
    const QTextBlock firstBlock = document->findBlock(position);
    if (!firstBlock.isValid())
        return false;

    region.first = firstBlock.blockNumber();
    if (region.first >= lines.size())
        return false;

    const std::optional<int> lastOld = lastLineOfRemovedText(region.first, position - firstBlock.position(), charsRemoved);
    const int lastNew = lastLineOfInsertedText(position, charsAdded);
    if (!lastOld || lastNew < region.first)
        return false;

    const int oldCount = *lastOld - region.first + 1;
    const int newCount = lastNew - region.first + 1;
    if (lines.size() - oldCount + newCount != document->blockCount())
        return false;

    region.oldLines = lines.mid(region.first, oldCount);
    region.oldIds = ids.mid(region.first, oldCount);
    if (!readNewLines(firstBlock, newCount, region.newLines))
        return false;

    pairLines(region.oldLines, region.oldIds, region.newLines, region.newIds, region.touched, nextId, allocateIds);
    if (!allocateIds && region.newIds.contains(-1))
        placeholderIds = true;
    return true;
}

/// Walks through the removed characters; every line break counts as one character
std::optional<int> EditHistory::lastLineOfRemovedText(int firstLine, int offsetInFirstLine, int charsRemoved) const
{
    int line = firstLine;
    int offset = offsetInFirstLine;
    int remaining = charsRemoved;
    while (true)
    {
        const int available = lines[line].size() - offset;
        if (available < 0)
            return std::nullopt;
        if (remaining <= available)
            return line;
        if (line + 1 >= lines.size())
        {
            // The range reaches the separator after the very last line. It is not a character of the text and cannot be
            // removed, but Qt reports it when the format of that separator changes (pressing Enter on an empty last line
            // does that, as a separate undo step).
            return remaining == available + 1 ? std::optional<int>(line) : std::nullopt;
        }
        remaining -= available + 1;
        ++line;
        offset = 0;
    }
}

int EditHistory::lastLineOfInsertedText(int position, int charsAdded) const
{
    const int lastPosition = document->characterCount() - 1; // the separator after the very last line
    const QTextBlock block = document->findBlock(std::min(position + charsAdded, lastPosition));
    return block.isValid() ? block.blockNumber() : -1;
}

bool EditHistory::readNewLines(const QTextBlock& firstBlock, int count, QList<QString>& newLines) const
{
    newLines.reserve(count);
    QTextBlock block = firstBlock;
    for (int i = 0; i < count; ++i, block = block.next())
    {
        if (!block.isValid())
            return false;
        newLines.append(block.text());
    }
    return true;
}

void EditHistory::applyRegion(const Region& region)
{
    replaceRange(lines, region.first, region.oldLines.size(), region.newLines);
    replaceRange(ids, region.first, region.oldIds.size(), region.newIds);
}

void EditHistory::beginStep(const Region& region)
{
    Step step;
    step.time = QDateTime::currentDateTime();
    step.lastTime = step.time;
    step.first = region.first;
    step.oldLines = region.oldLines;
    step.newLines = region.newLines;
    step.oldIds = region.oldIds;
    step.newIds = region.newIds;

    steps.push_back(std::move(step));
    current = static_cast<int>(steps.size());
    retained += steps.back().characters();
    recordTouches(steps.back(), current, region.touched);
}

/// Qt merged one more change (typing goes character by character) into the undo step we already have
void EditHistory::extendStep(Step& step, const Region& region)
{
    const qint64 before = step.characters();

    const int stepEnd = step.first + static_cast<int>(step.newIds.size()); // in the document as it is now
    const int regionEnd = region.first + static_cast<int>(region.oldIds.size());
    growStepToCover(step, std::min(step.first, region.first), std::max(stepEnd, regionEnd));

    const int offset = region.first - step.first;
    replaceRange(step.newIds, offset, static_cast<int>(region.oldIds.size()), region.newIds);
    if (!step.detailsDiscarded)
    {
        replaceRange(step.newLines, offset, static_cast<int>(region.oldLines.size()), region.newLines);
        step.summary.reset();
    }

    step.lastTime = QDateTime::currentDateTime();
    retained += step.characters() - before;
    recordTouches(step, current, region.touched);
}

/// Qt merged one more change (typing goes character by character) into the undo step we already have, and the change
/// reaches lines the step had not touched so far: they are the same before and after the step.
/// (`lines` and `ids` still describe the document before the new change, which is what is needed here.)
void EditHistory::growStepToCover(Step& step, int top, int bottom)
{
    const bool keepText = !step.detailsDiscarded;
    const int stepEnd = step.first + static_cast<int>(step.newIds.size());

    if (top < step.first)
    {
        const int count = step.first - top;
        step.oldIds = ids.mid(top, count) + step.oldIds;
        step.newIds = ids.mid(top, count) + step.newIds;
        if (keepText)
        {
            step.oldLines = lines.mid(top, count) + step.oldLines;
            step.newLines = lines.mid(top, count) + step.newLines;
        }
        step.first = top;
    }
    if (bottom > stepEnd)
    {
        const int count = bottom - stepEnd;
        step.oldIds += ids.mid(stepEnd, count);
        step.newIds += ids.mid(stepEnd, count);
        if (keepText)
        {
            step.oldLines += lines.mid(stepEnd, count);
            step.newLines += lines.mid(stepEnd, count);
        }
    }
}

void EditHistory::recordTouches(Step& step, int number, const QList<int>& touchedIds)
{
    for (const int id : touchedIds)
    {
        QList<int>& list = touches[id];
        if (list.isEmpty() || list.last() != number)
        {
            list.append(number);
            step.touchedIds.append(id);
        }
    }
}

/// New edit after an undo: what could have been redone is gone (this is what Qt does with its stack too)
void EditHistory::dropRedoBranch()
{
    const int total = static_cast<int>(steps.size());
    if (total <= current)
        return;

    for (int number = total; number > current; --number)
    {
        const Step& step = steps[number - 1];
        for (const int id : step.touchedIds)
        {
            const auto it = touches.find(id);
            if (it == touches.end())
                continue;
            while (!it->isEmpty() && it->last() >= number)
                it->removeLast();
            if (it->isEmpty())
                touches.erase(it);
        }
        retained -= step.characters();
    }

    steps.resize(current);
    for (SaveMark& mark : saves)
    {
        if (mark.state > current)
            mark.stateDiscarded = true;
    }
}

void EditHistory::finishOperation()
{
    const Operation finished = operation;
    operation = Operation::Edit;
    newStepPending = false;

    if (operationEvents == 0)
        return; // there was nothing to undo / redo

    operationEvents = 0;
    const int total = static_cast<int>(steps.size());

    if (finished == Operation::Undo)
    {
        if (current <= 0)
        {
            resync("undo without a step");
            return;
        }
        restoreIds(steps[current - 1], true);
        --current;
    }
    else
    {
        if (current >= total)
        {
            resync("redo without a step");
            return;
        }
        restoreIds(steps[current], false);
        ++current;
    }

    assignMissingIds();
    emit changed();
}

/// After undo/redo the document is what it was: give the lines their identities back (and with them their history)
bool EditHistory::restoreIds(const Step& step, bool oldSide)
{
    const QList<QString>& texts = oldSide ? step.oldLines : step.newLines;
    const QList<int>& stepIds = oldSide ? step.oldIds : step.newIds;
    const int count = static_cast<int>(stepIds.size());

    if (step.first + count > lines.size() || lines.size() != ids.size())
        return false;
    if (!step.detailsDiscarded)
    {
        for (int i = 0; i < count; ++i)
        {
            if (lines[step.first + i] != texts[i])
                return false;
        }
    }

    for (int i = 0; i < count; ++i)
        ids[step.first + i] = stepIds[i];
    return true;
}

void EditHistory::assignMissingIds()
{
    if (!placeholderIds)
        return;
    placeholderIds = false;
    for (int& id : ids)
    {
        if (id < 0)
            id = nextId++;
    }
}

void EditHistory::enforceBudget()
{
    if (retained <= budget)
        return;

    const qint64 target = budget / 4 * 3;
    const int open = current - 1; // new typing may still be merged into it
    for (int i = 0; i < static_cast<int>(steps.size()) && retained > target; ++i)
    {
        if (i != open && !steps[i].detailsDiscarded)
            discardDetails(steps[i]);
    }
}

void EditHistory::discardDetails(Step& step)
{
    summaryOf(step); // computes the numbers while the text is still here: they stay, the text goes
    step.summary->detailsDiscarded = true;
    retained -= step.characters();
    step.oldLines.clear();
    step.oldLines.squeeze();
    step.newLines.clear();
    step.newLines.squeeze();
    step.detailsDiscarded = true;
}

const EditHistory::StepInfo& EditHistory::summaryOf(const Step& step) const
{
    if (!step.summary)
        step.summary = computeSummary(step);
    return *step.summary;
}

EditHistory::StepInfo EditHistory::computeSummary(const Step& step) const
{
    StepInfo info;
    info.time = step.time;
    info.lastEditTime = step.lastTime;
    info.detailsDiscarded = step.detailsDiscarded;
    if (step.detailsDiscarded)
        return info; // cannot happen: the summary is computed before the text is dropped

    QHash<int, int> oldIndexOfId;
    for (int i = 0; i < step.oldIds.size(); ++i)
        oldIndexOfId.insert(step.oldIds[i], i);
    const QSet<int> newIds(step.newIds.begin(), step.newIds.end());

    QList<int> modified;
    QList<int> added;
    QList<int> removed;
    for (int j = 0; j < step.newIds.size(); ++j)
    {
        const auto it = oldIndexOfId.constFind(step.newIds[j]);
        if (it == oldIndexOfId.constEnd())
            added.append(step.first + j + 1);
        else if (step.newLines[j] != step.oldLines[it.value()])
            modified.append(step.first + j + 1);
    }
    for (int i = 0; i < step.oldIds.size(); ++i)
    {
        if (!newIds.contains(step.oldIds[i]))
            removed.append(step.first + i + 1);
    }

    info.modifiedLines = static_cast<int>(modified.size());
    info.addedLines = static_cast<int>(added.size());
    info.removedLines = static_cast<int>(removed.size());
    info.modified = toRanges(modified);
    info.added = toRanges(added);
    info.removed = toRanges(removed);

    const DiffCalculation::CharChangeCounts counts = DiffCalculation::countCharChanges(
        step.oldLines.join(QLatin1Char('\n')), step.newLines.join(QLatin1Char('\n')));
    info.charsInserted = counts.inserted;
    info.charsRemoved = counts.removed;
    info.charsApproximate = counts.approximate;
    return info;
}

int EditHistory::discardedStepCount() const
{
    return static_cast<int>(std::count_if(steps.begin(), steps.end(), [](const Step& step) { return step.detailsDiscarded; }));
}

EditHistory::StepInfo EditHistory::stepInfo(int number) const
{
    StepInfo info;
    if (number >= 1 && number <= stepCount())
    {
        info = summaryOf(steps[number - 1]);
        info.detailsDiscarded = steps[number - 1].detailsDiscarded;
    }
    else
    {
        info.time = baseline;
        info.lastEditTime = baseline;
    }

    info.number = std::max(number, 0);
    info.applied = number <= current;
    info.isCurrent = number == current;
    for (const SaveMark& mark : saves)
    {
        if (!mark.stateDiscarded && mark.state == number)
            info.saves.append(mark.time);
    }
    return info;
}

QList<DiffCalculation::LineDiffResult> EditHistory::stepDiff(int number) const
{
    if (number < 1 || number > stepCount())
        return {};
    const Step& step = steps[number - 1];
    if (step.detailsDiscarded)
        return {};

    return DiffCalculation::computeAllLineDiffs(diffLinesOf(step));
}

/// The lines of the step in the order of the document, each one unchanged, modified, added or removed
std::vector<DiffCalculation::DiffLine> EditHistory::diffLinesOf(const Step& step) const
{
    const QSet<int> oldIdSet(step.oldIds.begin(), step.oldIds.end());
    const QSet<int> newIdSet(step.newIds.begin(), step.newIds.end());

    using DiffCalculation::DiffLine;
    using DiffCalculation::DiffType;
    std::vector<DiffLine> diffLines;

    const int oldCount = static_cast<int>(step.oldIds.size());
    const int newCount = static_cast<int>(step.newIds.size());
    int i = 0;
    int j = 0;
    while (i < oldCount || j < newCount)
    {
        if (i < oldCount && j < newCount && step.oldIds[i] == step.newIds[j])
        {
            const bool same = step.oldLines[i] == step.newLines[j];
            diffLines.push_back({ step.first + i, step.first + j, step.oldLines[i], step.newLines[j],
                                  same ? DiffType::Unchanged : DiffType::Modified });
            ++i;
            ++j;
        }
        else if (j < newCount && !oldIdSet.contains(step.newIds[j]))
        {
            diffLines.push_back({ -1, step.first + j, QString(), step.newLines[j], DiffType::Added });
            ++j;
        }
        else if (i < oldCount && !newIdSet.contains(step.oldIds[i]))
        {
            diffLines.push_back({ step.first + i, -1, step.oldLines[i], QString(), DiffType::Removed });
            ++i;
        }
        else if (i < oldCount)
        {
            diffLines.push_back({ step.first + i, -1, step.oldLines[i], QString(), DiffType::Removed }); // out of order: cannot happen
            ++i;
        }
        else
        {
            diffLines.push_back({ -1, step.first + j, QString(), step.newLines[j], DiffType::Added });
            ++j;
        }
    }

    return diffLines;
}

int EditHistory::stepLineInEditor(int number) const
{
    if (number < 1 || number > stepCount() || lines.isEmpty())
        return -1;
    const Step& step = steps[number - 1];

    QHash<int, int> oldIndexOfId;
    for (int i = 0; i < step.oldIds.size(); ++i)
        oldIndexOfId.insert(step.oldIds[i], i);

    // The first line the step changed which is still in the document
    int checked = 0;
    for (int j = 0; j < step.newIds.size() && checked < 64; ++j)
    {
        const auto it = oldIndexOfId.constFind(step.newIds[j]);
        const bool changed = it == oldIndexOfId.constEnd() || step.detailsDiscarded
                             || step.newLines[j] != step.oldLines[it.value()];
        if (!changed)
            continue;
        ++checked;
        const int line = lineOfId(step.newIds[j]);
        if (line >= 0)
            return line;
    }

    // Nothing of it is in the document now (a removal, or the step is undone): go to the place where it happened
    return std::min(step.first, static_cast<int>(lines.size()) - 1);
}

int EditHistory::lineId(int line) const
{
    return (line >= 0 && line < ids.size()) ? ids[line] : -1;
}

int EditHistory::lineOfId(int id) const
{
    return id < 0 ? -1 : static_cast<int>(ids.indexOf(id));
}

EditHistory::LineMarker EditHistory::lineMarker(int line) const
{
    const int id = lineId(line);
    if (id < 0)
        return LineMarker::None;
    const auto it = touches.constFind(id);
    if (it == touches.constEnd() || it->isEmpty())
        return LineMarker::None;
    return it->first() <= current ? LineMarker::Applied : LineMarker::UndoneOnly;
}

int EditHistory::lineChangeCount(int line) const
{
    const auto it = touches.constFind(lineId(line));
    return it == touches.constEnd() ? 0 : static_cast<int>(it->size());
}

EditHistory::LineHistory EditHistory::lineHistoryById(int id) const
{
    LineHistory history;
    history.id = id;
    history.line = lineOfId(id);
    history.currentIndex = current;
    history.stepCount = stepCount();

    const auto it = touches.constFind(id);
    if (id < 0 || it == touches.constEnd())
        return history;

    for (const int number : *it)
    {
        if (number >= 1 && number <= stepCount())
            history.entries.append(lineEntry(id, number));
    }
    return history;
}

EditHistory::LineEntry EditHistory::lineEntry(int id, int stepNumber) const
{
    const Step& step = steps[stepNumber - 1];

    LineEntry entry;
    entry.step = stepNumber;
    entry.time = step.time;
    entry.applied = stepNumber <= current;
    entry.isCurrent = stepNumber == current;
    entry.detailsDiscarded = step.detailsDiscarded;

    const qsizetype oldIndex = step.oldIds.indexOf(id);
    const qsizetype newIndex = step.newIds.indexOf(id);
    entry.kind = oldIndex < 0 ? LineKind::Added : (newIndex < 0 ? LineKind::Removed : LineKind::Modified);
    if (!step.detailsDiscarded)
    {
        if (oldIndex >= 0)
            entry.oldText = step.oldLines[oldIndex];
        if (newIndex >= 0)
            entry.newText = step.newLines[newIndex];
    }

    for (const SaveMark& mark : saves)
    {
        if (!mark.stateDiscarded && mark.state >= stepNumber)
        {
            entry.savedAt = mark.time; // the first write which had this change in it
            break;
        }
    }
    return entry;
}

bool EditHistory::verifyConsistency(QString* problem) const
{
    auto fail = [problem](const QString& text) {
        if (problem)
            *problem = text;
        return false;
    };

    if (lines.size() != document->blockCount())
        return fail(QStringLiteral("line count %1, document has %2 blocks").arg(lines.size()).arg(document->blockCount()));
    int index = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next(), ++index)
    {
        if (lines[index] != block.text())
            return fail(QStringLiteral("line %1 differs from the document").arg(index + 1));
    }

    if (ids.size() != lines.size())
        return fail(QStringLiteral("ids and lines have different sizes"));
    const QSet<int> uniqueIds(ids.begin(), ids.end());
    if (uniqueIds.size() != ids.size())
        return fail(QStringLiteral("line identities are not unique"));
    for (const int id : ids)
    {
        if (id < 0 || id >= nextId)
            return fail(QStringLiteral("line identity %1 out of range").arg(id));
    }

    if (current < 0 || current > stepCount())
        return fail(QStringLiteral("current index %1 out of range 0..%2").arg(current).arg(stepCount()));

    for (auto it = touches.constBegin(); it != touches.constEnd(); ++it)
    {
        const QList<int>& list = it.value();
        if (list.isEmpty())
            return fail(QStringLiteral("empty list of touches for id %1").arg(it.key()));
        for (int i = 0; i < list.size(); ++i)
        {
            if (list[i] < 1 || list[i] > stepCount() || (i > 0 && list[i] <= list[i - 1]))
                return fail(QStringLiteral("bad list of touches for id %1").arg(it.key()));
        }
    }

    qint64 characters = 0;
    for (const Step& step : steps)
    {
        if (step.oldIds.size() != step.oldLines.size() && !step.detailsDiscarded)
            return fail(QStringLiteral("a step has old ids and old lines of different sizes"));
        if (step.newIds.size() != step.newLines.size() && !step.detailsDiscarded)
            return fail(QStringLiteral("a step has new ids and new lines of different sizes"));
        characters += step.characters();
    }
    if (characters != retained)
        return fail(QStringLiteral("retained characters %1, counted %2").arg(retained).arg(characters));

    return true;
}
