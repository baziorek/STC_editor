#include "ChangeGutterWidget.h"

#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextDocument>

ChangeGutterWidget::ChangeGutterWidget(QWidget *parent) : QWidget(parent)
{
    setFixedWidth(52);
    setMouseTracking(true);
}

qreal ChangeGutterWidget::regionCenterY(const SideBySideDiffWidget::ChangeRegion &region) const
{
    if (!document)
        return -1;
    const auto *layout = document->documentLayout();
    const QTextBlock first = document->findBlockByNumber(region.firstEntry);
    const QTextBlock last = document->findBlockByNumber(region.lastEntry);
    if (!first.isValid() || !last.isValid())
        return -1;

    const qreal y1 = layout->blockBoundingRect(first).top() - scrollOffset;
    const qreal y2 = layout->blockBoundingRect(last).bottom() - scrollOffset;
    return (y1 + y2) / 2;
}

int ChangeGutterWidget::hitTest(const QPoint &pos, int *regionIndex) const
{
    if (!regions || actions == SideBySideDiffWidget::Actions::None)
        return 0;

    for (int i = 0; i < regions->size(); ++i)
    {
        const qreal yc = regionCenterY((*regions)[i]);
        if (yc < -12 || yc > height() + 12)
            continue;

        const QRect button = buttonRect(yc);
        if (button.contains(pos))
        {
            if (regionIndex)
                *regionIndex = i;
            if (actions == SideBySideDiffWidget::Actions::RestoreOnly)
                return 3;
            return pos.x() < width() / 2 ? 1 : 2;
        }
    }
    return 0;
}

QRect ChangeGutterWidget::buttonRect(qreal yc) const
{
    if (actions == SideBySideDiffWidget::Actions::RestoreOnly)
        return QRect(width() / 2 - 11, int(yc) - 9, 22, 18);
    return QRect(3, int(yc) - 9, width() - 6, 18);
}

void ChangeGutterWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!regions || actions == SideBySideDiffWidget::Actions::None)
        return;

    const QPen gridPen(QColor(200, 200, 200));
    painter.setPen(gridPen);
    painter.drawLine(0, 0, 0, height());

    for (int i = 0; i < regions->size(); ++i)
    {
        const qreal yc = regionCenterY((*regions)[i]);
        if (yc < -12 || yc > height() + 12)
            continue;

        const QRect rect = buttonRect(yc);
        painter.setBrush(QColor(240, 244, 250));
        painter.setPen(QColor(120, 130, 145));
        painter.drawRoundedRect(rect, 4, 4);

        painter.setPen(QColor(40, 45, 55));
        if (actions == SideBySideDiffWidget::Actions::RestoreOnly)
        {
            painter.drawText(rect, Qt::AlignCenter, QStringLiteral("↩"));
        }
        else
        {
            // left button takes the left-hand version, right button the right-hand one
            const int mid = width() / 2;
            painter.drawText(QRect(3, rect.top(), mid - 3, rect.height()), Qt::AlignCenter, QStringLiteral("←"));
            painter.drawText(QRect(mid, rect.top(), width() - mid - 3, rect.height()), Qt::AlignCenter, QStringLiteral("→"));
        }
    }
}

void ChangeGutterWidget::mousePressEvent(QMouseEvent *event)
{
    int regionIndex = -1;
    const int hit = hitTest(event->position().toPoint(), &regionIndex);
    if (hit == 1 && onTakeLeft && regionIndex >= 0)
        onTakeLeft(regionIndex);
    else if (hit == 2 && onTakeRight && regionIndex >= 0)
        onTakeRight(regionIndex);
    else if (hit == 3 && onTakeLeft && regionIndex >= 0)
        onTakeLeft(regionIndex);
}

void ChangeGutterWidget::mouseMoveEvent(QMouseEvent *event)
{
    setCursor(hitTest(event->position().toPoint()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
