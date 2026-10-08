#include "EditHistoryFormat.h"

#include <QCoreApplication>
#include <QDate>

namespace
{
const QString insertedStyle = QStringLiteral("color:#116329;background-color:#aceebb");
const QString deletedStyle = QStringLiteral("color:#82071e;background-color:#ffc1ba;text-decoration:line-through");

constexpr int keptAroundChange = 60; // how much of an unchanged stretch is shown next to a change

QString html(const QString& text)
{
    return text.toHtmlEscaped();
}

QString wrapped(const QString& body)
{
    return QStringLiteral("<span style=\"white-space:pre-wrap\">") + body + QStringLiteral("</span>");
}

QString clipped(const QString& text, bool first, bool last)
{
    if (text.size() <= 2 * keptAroundChange + 10)
        return text;

    auto safeLeft = [&text](int count) {
        if (count > 0 && count < text.size() && text[count - 1].isHighSurrogate())
            ++count;
        return text.left(count);
    };
    auto safeRight = [&text](int count) {
        if (count > 0 && count < text.size() && text[text.size() - count].isLowSurrogate())
            ++count;
        return text.right(count);
    };

    const QString ellipsis = QStringLiteral("…");
    if (first && last)
        return safeLeft(2 * keptAroundChange) + ellipsis;
    if (first)
        return ellipsis + safeRight(keptAroundChange);
    if (last)
        return safeLeft(keptAroundChange) + ellipsis;
    return safeLeft(keptAroundChange) + ellipsis + safeRight(keptAroundChange);
}

QString plainGroup(const QString& sign, const QList<EditHistory::LineRange>& ranges)
{
    if (ranges.isEmpty())
        return {};
    return sign + EditHistoryFormat::lineRanges(ranges);
}
} // namespace

namespace EditHistoryFormat
{
QString colorForInserted()
{
    return QStringLiteral("#116329");
}

QString colorForDeleted()
{
    return QStringLiteral("#82071e");
}

QString time(const QDateTime& time)
{
    if (!time.isValid())
        return QStringLiteral("–");
    return time.date() == QDate::currentDate() ? time.toString(QStringLiteral("HH:mm:ss"))
                                               : time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString lineRanges(const QList<EditHistory::LineRange>& ranges, int maxRanges)
{
    QStringList parts;
    for (const auto& range : ranges)
    {
        if (parts.size() == maxRanges)
        {
            parts << QStringLiteral("…");
            break;
        }
        parts << (range.first == range.last ? QString::number(range.first)
                                            : QStringLiteral("%1–%2").arg(range.first).arg(range.last));
    }
    return parts.join(QStringLiteral(", "));
}

QString lineNumbersPlain(const EditHistory::StepInfo& info)
{
    if (info.changedLines() == 0)
        return info.number == 0 ? QCoreApplication::translate("EditHistoryFormat", "(initial state)")
                                : QCoreApplication::translate("EditHistoryFormat", "(no text changed)");

    QStringList groups;
    for (const QString& group : { plainGroup(QString(), info.modified),
                                  plainGroup(QStringLiteral("+"), info.added),
                                  plainGroup(QStringLiteral("−"), info.removed) })
    {
        if (!group.isEmpty())
            groups << group;
    }
    return groups.join(QStringLiteral("   "));
}

QString lineNumbersHtml(const EditHistory::StepInfo& info)
{
    if (info.changedLines() == 0)
        return QStringLiteral("<i>") + html(lineNumbersPlain(info)) + QStringLiteral("</i>");

    QStringList groups;
    if (!info.modified.isEmpty())
        groups << html(lineRanges(info.modified));
    if (!info.added.isEmpty())
        groups << QStringLiteral("<span style=\"color:%1\">+%2</span>").arg(colorForInserted(), html(lineRanges(info.added)));
    if (!info.removed.isEmpty())
        groups << QStringLiteral("<span style=\"color:%1\">−%2</span>").arg(colorForDeleted(), html(lineRanges(info.removed)));
    return wrapped(groups.join(QStringLiteral("&nbsp;&nbsp;&nbsp;")));
}

QString charsPlain(const EditHistory::StepInfo& info)
{
    QStringList parts;
    if (info.charsInserted > 0)
        parts << QStringLiteral("+%1").arg(info.charsInserted);
    if (info.charsRemoved > 0)
        parts << QStringLiteral("−%1").arg(info.charsRemoved);
    if (parts.isEmpty())
        return QStringLiteral("0");
    return (info.charsApproximate ? QStringLiteral("≈ ") : QString()) + parts.join(QLatin1Char(' '));
}

QString charsHtml(const EditHistory::StepInfo& info)
{
    QStringList parts;
    if (info.charsInserted > 0)
        parts << QStringLiteral("<span style=\"color:%1\">+%2</span>").arg(colorForInserted()).arg(info.charsInserted);
    if (info.charsRemoved > 0)
        parts << QStringLiteral("<span style=\"color:%1\">−%2</span>").arg(colorForDeleted()).arg(info.charsRemoved);
    if (parts.isEmpty())
        return QStringLiteral("0");
    return (info.charsApproximate ? QStringLiteral("≈ ") : QString()) + parts.join(QLatin1Char(' '));
}

QString insertedHtml(const QString& text)
{
    return QStringLiteral("<span style=\"%1\">%2</span>").arg(insertedStyle, html(text));
}

QString deletedHtml(const QString& text)
{
    return QStringLiteral("<span style=\"%1\">%2</span>").arg(deletedStyle, html(text));
}

QString inlineDiffHtml(const QString& oldText, const QString& newText)
{
    using DiffCalculation::FragmentType;
    const auto fragments = DiffCalculation::computeInlineDiff(oldText, newText);

    QString body;
    for (int i = 0; i < fragments.size(); ++i)
    {
        const auto& fragment = fragments[i];
        switch (fragment.type)
        {
        case FragmentType::Equal:
            body += html(clipped(fragment.text, i == 0, i == fragments.size() - 1));
            break;
        case FragmentType::Insert:
            body += QStringLiteral("<span style=\"%1\">%2</span>").arg(insertedStyle, html(fragment.text));
            break;
        case FragmentType::Delete:
            body += QStringLiteral("<span style=\"%1\">%2</span>").arg(deletedStyle, html(fragment.text));
            break;
        }
    }
    return wrapped(body);
}
} // namespace EditHistoryFormat
