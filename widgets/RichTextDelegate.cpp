#include "RichTextDelegate.h"

#include <QTableView>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <cmath>
#include <QPainter>
#include <QTextDocument>

namespace
{
constexpr int horizontalPadding = 4;
constexpr int verticalPadding = 2;
} // namespace

RichTextDelegate::RichTextDelegate(bool wrap, QObject* parent)
    : QStyledItemDelegate(parent), wrap(wrap)
{
}

void RichTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QVariant html = index.data(RichTextRole);
    if (!html.isValid() || (plainWhenSelected && (option.state & QStyle::State_Selected)))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear(); // the background, selection and focus are drawn by the style; the text by us

    const QWidget* widget = opt.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    QTextDocument document;
    document.setDefaultFont(opt.font);
    document.setDocumentMargin(0);
    document.setHtml(html.toString());

    const QRect area = opt.rect.adjusted(horizontalPadding, verticalPadding, -horizontalPadding, -verticalPadding);
    if (wrap)
        document.setTextWidth(std::max(10, area.width()));

    QColor textColor = opt.palette.color(QPalette::Text);
    if (opt.state & QStyle::State_Selected)
    {
        textColor = opt.palette.color(QPalette::HighlightedText);
    }
    else
    {
        const QVariant foreground = index.data(Qt::ForegroundRole); // e.g. grey for what has been undone
        if (foreground.canConvert<QBrush>())
            textColor = qvariant_cast<QBrush>(foreground).color();
    }
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, textColor);

    painter->save();
    painter->setClipRect(opt.rect);
    const qreal offsetY = wrap ? 0 : std::max<qreal>(0, (area.height() - document.size().height()) / 2);
    painter->translate(area.left(), area.top() + offsetY);
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

QSize RichTextDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QVariant html = index.data(RichTextRole);
    if (!html.isValid())
        return QStyledItemDelegate::sizeHint(option, index);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QTextDocument document;
    document.setDefaultFont(opt.font);
    document.setDocumentMargin(0);
    document.setHtml(html.toString());

    if (wrap)
    {
        int columnWidth = 300;
        if (const auto* view = qobject_cast<const QTableView*>(option.widget))
            columnWidth = view->columnWidth(index.column());
        document.setTextWidth(std::max(10, columnWidth - 2 * horizontalPadding));
    }
    else
    {
        document.setTextWidth(-1);
    }

    return QSize(static_cast<int>(std::ceil(document.idealWidth())) + 2 * horizontalPadding,
                 static_cast<int>(std::ceil(document.size().height())) + 2 * verticalPadding);
}
