#pragma once

#include <QWidget>
#include <QVector>
#include <functional>
#include <vector>

#include "utils/DiffCalculation.h"
#include "SideBySideDiffWidget.h"

class QTextDocument;

/**
 * @class ChangeGutterWidget
 * @brief The column between the panes of SideBySideDiffWidget with the
 * "take left" / "take right" buttons of every change run.
 *
 * It does not scroll itself: it is as high as the viewport and the arrows are painted at the
 * place of their run, translated by the scroll offset of the panes.
 */
class ChangeGutterWidget : public QWidget
{
public:
    const QVector<SideBySideDiffWidget::ChangeRegion> *regions = nullptr;
    const std::vector<DiffCalculation::DiffLine> *diffLines = nullptr;
    QTextDocument *document = nullptr; ///< the document of the left pane (both have the same rows)
    int scrollOffset = 0;
    SideBySideDiffWidget::Actions actions = SideBySideDiffWidget::Actions::None;
    std::function<void(int)> onTakeLeft;   ///< region index
    std::function<void(int)> onTakeRight;

    explicit ChangeGutterWidget(QWidget *parent = nullptr);

    /// The vertical center of a change run in gutter coordinates, or -1 when it is not visible
    qreal regionCenterY(const SideBySideDiffWidget::ChangeRegion &region) const;

    /// Which button of the region is under the point: 0 = none, 1 = left ("take left"), 2 = right ("take right"), 3 = restore
    int hitTest(const QPoint &pos, int *regionIndex = nullptr) const;
    QRect buttonRect(qreal yc) const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
};
