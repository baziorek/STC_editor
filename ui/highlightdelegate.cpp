#include <QFlags>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QVariant>
#include <Qt>
#include <QtGlobal>
#include <algorithm>
#include "highlightdelegate.h"


namespace
{
struct Span
{
    int start;
    int length;
};

/// Places of all the terms in the text (ignoring the case), from left to right. Places which overlap or touch each other
/// (the terms `ala` and `al` in `Ala`) make one span, so the formats given to the layout never overlap.
QList<Span> spansOfTerms(const QString& text, const QStringList& terms)
{
    QList<Span> spans;
    for (const QString& term : terms)
    {
        if (term.isEmpty())
            continue;

        for (qsizetype idx = text.indexOf(term, 0, Qt::CaseInsensitive); idx >= 0;
             idx = text.indexOf(term, idx + term.length(), Qt::CaseInsensitive))
        {
            spans.append({ static_cast<int>(idx), static_cast<int>(term.length()) });
        }
    }

    std::sort(spans.begin(), spans.end(), [](const Span& left, const Span& right) {
        return left.start < right.start;
    });

    QList<Span> merged;
    for (const Span& span : spans)
    {
        if (!merged.isEmpty() && span.start <= merged.last().start + merged.last().length)
        {
            Span& previous = merged.last();
            previous.length = std::max(previous.length, span.start + span.length - previous.start);
        }
        else
        {
            merged.append(span);
        }
    }
    return merged;
}
} // namespace


HighlightDelegate::HighlightDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void HighlightDelegate::setSearchTerms(const QStringList& terms)
{
    searchTerms = terms;
}

void HighlightDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();

    QString text = index.data().toString();
    QRect rect = option.rect;

    // Base format:
    QTextLayout layout(text, option.font);

    // Find all occurences to make more visible:
    QList<QTextLayout::FormatRange> formats;

    QTextCharFormat highlight;
    highlight.setBackground(Qt::yellow);
    highlight.setForeground(Qt::black);
    highlight.setFontUnderline(true);
    for (const Span& span : spansOfTerms(text, searchTerms))
        formats.append({ span.start, span.length, highlight });

    // Building text layout:
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::NoWrap);
    layout.setTextOption(textOption);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid())
    {
        line.setLineWidth(rect.width());
        line.setPosition(QPointF(0, 0));
    }
    layout.endLayout();

    // Set up brush and background:
    if (option.state & QStyle::State_Selected)
        painter->fillRect(rect, option.palette.highlight());
    else
        painter->fillRect(rect, option.backgroundBrush);

    // Move and paint
    painter->translate(rect.topLeft());
    layout.draw(painter, QPointF(0, (rect.height() - line.height()) / 2), formats);
    painter->restore();
}
