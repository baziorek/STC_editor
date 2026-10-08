#pragma once

#include <QDateTime>
#include <QString>

#include "utils/EditHistory.h"

/// How the windows of the edit history write down times, line numbers and changes (one place, so both agree)
namespace EditHistoryFormat
{
/// "14:03:22", and with the date when it is not today
QString time(const QDateTime& time);

/// "12, 14–16, 20" (and "…" when there are more ranges than `maxRanges`)
QString lineRanges(const QList<EditHistory::LineRange>& ranges, int maxRanges = 10);

/// Line numbers of a step: modified "12, 14–16", added "+20–21", removed "−30" (numbers before the step)
QString lineNumbersPlain(const EditHistory::StepInfo& info);
QString lineNumbersHtml(const EditHistory::StepInfo& info);

/// "+12 −3"
QString charsPlain(const EditHistory::StepInfo& info);
QString charsHtml(const EditHistory::StepInfo& info);

/// One line with the change in it: equal text as is, inserted text green, deleted text red and struck through.
/// Long unchanged stretches are shortened with "…" so that the change is what is seen.
QString inlineDiffHtml(const QString& oldText, const QString& newText);
QString insertedHtml(const QString& text);
QString deletedHtml(const QString& text);

QString colorForInserted();
QString colorForDeleted();
} // namespace EditHistoryFormat
