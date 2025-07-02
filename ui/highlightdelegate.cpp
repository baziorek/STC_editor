#include <QPainter>
#include <QTextLayout>
#include "highlightdelegate.h"


HighlightDelegate::HighlightDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void HighlightDelegate::setSearchTerm(const QString& term)
{
    searchTerm = term;
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

    if (!searchTerm.isEmpty())
    {
        int idx = text.indexOf(searchTerm, 0, Qt::CaseInsensitive);
        while (idx >= 0) {
            QTextLayout::FormatRange r;
            r.start = idx;
            r.length = searchTerm.length();

            QTextCharFormat f;
            f.setBackground(Qt::yellow);
            f.setForeground(Qt::black);
            f.setFontUnderline(true);
            formats.append({ idx, static_cast<int>(searchTerm.length()), f });

            idx = text.indexOf(searchTerm, idx + searchTerm.length(), Qt::CaseInsensitive);
        }
    }

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
