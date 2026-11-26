#include "SideBySideDiffWidget.h"

#include <QAbstractTextDocumentLayout>
#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>
#include <Qt>

#include <functional>

namespace
{

/// The column between the panes with the "take left" / "take right" buttons of every change run.
/// It does not scroll itself: it is as high as the viewport and the arrows are painted at the
/// place of their run, translated by the scroll offset of the panes.
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

    explicit ChangeGutterWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedWidth(52);
        setMouseTracking(true);
    }

    /// The vertical center of a change run in gutter coordinates, or -1 when it is not visible
    qreal regionCenterY(const SideBySideDiffWidget::ChangeRegion &region) const
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

    /// Which button of the region is under the point: 0 = none, 1 = left ("take left"), 2 = right ("take right"), 3 = restore
    int hitTest(const QPoint &pos, int *regionIndex = nullptr) const
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

    QRect buttonRect(qreal yc) const
    {
        if (actions == SideBySideDiffWidget::Actions::RestoreOnly)
            return QRect(width() / 2 - 11, int(yc) - 9, 22, 18);
        return QRect(3, int(yc) - 9, width() - 6, 18);
    }

protected:
    void paintEvent(QPaintEvent *) override
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

    void mousePressEvent(QMouseEvent *event) override
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

    void mouseMoveEvent(QMouseEvent *event) override
    {
        setCursor(hitTest(event->position().toPoint()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
};

QTextEdit *makeDiffPane(QWidget *parent, const QString &objectName)
{
    auto *edit = new QTextEdit(parent);
    edit->setObjectName(objectName);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QTextEdit::NoWrap);
    edit->setFrameShape(QFrame::NoFrame);
    edit->document()->setDocumentMargin(0);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    return edit;
}

} // namespace


SideBySideDiffWidget::SideBySideDiffWidget(QWidget *parent) : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- header: change navigation and the titles of the two sides ---
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(6, 4, 6, 4);

    previousButton_ = new QPushButton(tr("◀ Prev change"), this);
    nextButton_ = new QPushButton(tr("Next change ▶"), this);
    changeLabel_ = new QLabel(this);

    headerLayout->addWidget(previousButton_);
    headerLayout->addWidget(nextButton_);
    headerLayout->addWidget(changeLabel_);
    headerLayout->addStretch();

    oldTitleLabel_ = new QLabel(this);
    oldTitleLabel_->setStyleSheet("font-weight: bold");
    headerLayout->addWidget(oldTitleLabel_);
    headerLayout->addSpacing(24);
    newTitleLabel_ = new QLabel(this);
    newTitleLabel_->setStyleSheet("font-weight: bold");
    headerLayout->addWidget(newTitleLabel_);

    mainLayout->addLayout(headerLayout);

    // --- panes: line numbers, texts, the gutter with the buttons ---
    auto *panesLayout = new QHBoxLayout();
    panesLayout->setContentsMargins(0, 0, 0, 0);
    panesLayout->setSpacing(0);

    numberOldEdit_ = makeDiffPane(this, QStringLiteral("numbersOld"));
    numberOldEdit_->setFixedWidth(52);
    numberOldEdit_->setTextInteractionFlags(Qt::NoTextInteraction);

    oldEdit_ = makeDiffPane(this, QStringLiteral("old"));

    gutter_ = new ChangeGutterWidget(this);
    gutter_->setObjectName(QStringLiteral("gutter"));
    auto *gutter = static_cast<ChangeGutterWidget *>(gutter_);
    gutter->actions = actionMode_;
    gutter->onTakeLeft = [this](int region) { emit regionDiscarded(region); };
    gutter->onTakeRight = [this](int region) { emit regionAccepted(region); };

    numberNewEdit_ = makeDiffPane(this, QStringLiteral("numbersNew"));
    numberNewEdit_->setFixedWidth(52);
    numberNewEdit_->setTextInteractionFlags(Qt::NoTextInteraction);

    newEdit_ = makeDiffPane(this, QStringLiteral("new"));

    panesLayout->addWidget(numberOldEdit_);
    panesLayout->addWidget(oldEdit_, 1);
    panesLayout->addWidget(gutter_);
    panesLayout->addWidget(numberNewEdit_);
    panesLayout->addWidget(newEdit_, 1);

    mainLayout->addLayout(panesLayout, 1);

    syncScrollBars();

    connect(previousButton_, &QPushButton::clicked, this, &SideBySideDiffWidget::previousChange);
    connect(nextButton_, &QPushButton::clicked, this, &SideBySideDiffWidget::nextChange);

    // a click in the right-hand pane asks the editor to show that line (like the old table did)
    connect(newEdit_, &QTextEdit::cursorPositionChanged, this, [this]() {
        const int row = newEdit_->textCursor().blockNumber();
        if (row >= 0 && row < static_cast<int>(diffLines_.size()) && diffLines_[row].newIndex >= 0)
            emit jumpToLineInEditor(diffLines_[row].newIndex);
    });
}

void SideBySideDiffWidget::setDiffLines(const std::vector<DiffCalculation::DiffLine> &diffLines)
{
    setDiffData(diffLines, DiffCalculation::computeAllLineDiffs(diffLines));
}

void SideBySideDiffWidget::setDiffData(const std::vector<DiffCalculation::DiffLine> &diffLines,
                                       const QList<DiffCalculation::LineDiffResult> &lineDiffs)
{
    diffLines_ = diffLines;
    lineDiffs_ = lineDiffs;

    // the runs of consecutive changed rows get the gutter buttons
    regions_.clear();
    for (size_t i = 0; i < diffLines_.size();)
    {
        if (diffLines_[i].type == DiffCalculation::DiffType::Unchanged)
        {
            ++i;
            continue;
        }
        size_t j = i;
        while (j + 1 < diffLines_.size() && diffLines_[j + 1].type != DiffCalculation::DiffType::Unchanged)
            ++j;
        regions_.append({static_cast<int>(i), static_cast<int>(j)});
        i = j + 1;
    }

    buildPanes();
    currentRegion_ = regions_.isEmpty() ? -1 : 0;
    highlightedFirst_ = highlightedLast_ = -1; // the panes were rebuilt - no highlight survives
    updateCurrentRegionHighlight();
    updateChangeLabel();
    updateGutter();
}

void SideBySideDiffWidget::setPlainLines(const QStringList &oldLines, const QStringList &newLines)
{
    diffLines_.clear();
    lineDiffs_.clear();
    regions_.clear();
    currentRegion_ = -1;
    highlightedFirst_ = highlightedLast_ = -1;

    buildPlainPanes(oldLines, newLines);

    changeLabel_->setText(tr("Computing diff…"));
    previousButton_->setEnabled(false);
    nextButton_->setEnabled(false);
    updateGutter();
}

void SideBySideDiffWidget::buildPanes()
{
    QStringList oldLines, newLines, oldNumbers, newNumbers;
    oldLines.reserve(static_cast<int>(diffLines_.size()));
    newLines.reserve(static_cast<int>(diffLines_.size()));
    oldNumbers.reserve(static_cast<int>(diffLines_.size()));
    newNumbers.reserve(static_cast<int>(diffLines_.size()));

    // a line missing on one side becomes an empty placeholder, so both documents have
    // exactly the same number of lines and always scroll together
    for (const auto &line : diffLines_)
    {
        oldLines << (line.oldIndex >= 0 ? line.oldText : QString());
        newLines << (line.newIndex >= 0 ? line.newText : QString());
        oldNumbers << (line.oldIndex >= 0 ? QString::number(line.oldIndex + 1) : QString());
        newNumbers << (line.newIndex >= 0 ? QString::number(line.newIndex + 1) : QString());
    }

    fillPanes(oldLines.join('\n'), newLines.join('\n'),
              oldNumbers.join('\n'), newNumbers.join('\n'));

    applyLineFormats(oldEdit_, /*oldSide=*/true);
    applyLineFormats(newEdit_, /*oldSide=*/false);

    setScrollValue(0);
}

void SideBySideDiffWidget::buildPlainPanes(const QStringList &oldLines, const QStringList &newLines)
{
    QStringList oldNumbers, newNumbers;
    oldNumbers.reserve(oldLines.size());
    newNumbers.reserve(newLines.size());
    for (int i = 1; i <= oldLines.size(); ++i)
        oldNumbers << QString::number(i);
    for (int i = 1; i <= newLines.size(); ++i)
        newNumbers << QString::number(i);

    fillPanes(oldLines.join('\n'), newLines.join('\n'),
              oldNumbers.join('\n'), newNumbers.join('\n'));

    setScrollValue(0);
}

void SideBySideDiffWidget::fillPanes(const QString &oldText, const QString &newText,
                                     const QString &oldNumbers, const QString &newNumbers)
{
    numberOldEdit_->setPlainText(oldNumbers);
    numberNewEdit_->setPlainText(newNumbers);
    oldEdit_->setPlainText(oldText);
    newEdit_->setPlainText(newText);

    // the numbers are gray and right-aligned
    for (QTextEdit *numbers : {numberOldEdit_, numberNewEdit_})
    {
        QTextCursor cursor(numbers->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat format;
        format.setForeground(QColor(120, 120, 120));
        cursor.mergeCharFormat(format);
        cursor.clearSelection();

        for (QTextBlock block = numbers->document()->begin(); block.isValid(); block = block.next())
        {
            QTextBlockFormat blockFormat = block.blockFormat();
            blockFormat.setAlignment(Qt::AlignRight);
            QTextCursor blockCursor(block);
            blockCursor.setBlockFormat(blockFormat);
        }
    }
}

void SideBySideDiffWidget::applyLineFormats(QTextEdit *edit, bool oldSide)
{
    QTextDocument *doc = edit->document();
    QTextCursor cursor(doc);

    const auto backgroundFor = [oldSide](DiffCalculation::DiffType type) -> QColor {
        using DiffCalculation::DiffType;
        if (type == DiffType::Unchanged)
            return QColor();
        if (oldSide)
            return type == DiffType::Removed ? QColor(255, 214, 214) : QColor(255, 236, 190);
        return type == DiffType::Added ? QColor(206, 240, 206) : QColor(255, 236, 190);
    };

    const auto charFormatFor = [oldSide]() {
        QTextCharFormat format;
        format.setBackground(oldSide ? QColor(255, 150, 150) : QColor(150, 230, 150));
        return format;
    };

    for (int row = 0; row < static_cast<int>(diffLines_.size()); ++row)
    {
        const auto &line = diffLines_[row];
        if (line.type == DiffCalculation::DiffType::Unchanged)
            continue;

        const QTextBlock block = doc->findBlockByNumber(row);
        if (!block.isValid())
            break;

        cursor.setPosition(block.position());

        QTextBlockFormat blockFormat = block.blockFormat();
        blockFormat.setBackground(backgroundFor(line.type));
        cursor.setBlockFormat(blockFormat);

        // the changed characters inside a modified line
        if (line.type == DiffCalculation::DiffType::Modified)
        {
            const auto &fragments = oldSide ? lineDiffs_[row].oldFragments : lineDiffs_[row].newFragments;
            const auto wanted = oldSide ? DiffCalculation::FragmentType::Delete
                                        : DiffCalculation::FragmentType::Insert;
            const QTextCharFormat changeFormat = charFormatFor();

            int position = block.position();
            for (const auto &fragment : fragments)
            {
                const int length = fragment.text.size();
                if (fragment.type == wanted && length > 0)
                {
                    cursor.setPosition(position);
                    cursor.setPosition(position + length, QTextCursor::KeepAnchor);
                    cursor.mergeCharFormat(changeFormat);
                }
                position += length;
            }
        }
    }
}

void SideBySideDiffWidget::syncScrollBars()
{
    const auto panes = {numberOldEdit_, oldEdit_, numberNewEdit_, newEdit_};
    for (QTextEdit *pane : panes)
    {
        connect(pane->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, pane](int value) {
            const auto others = {numberOldEdit_, oldEdit_, numberNewEdit_, newEdit_};
            for (QTextEdit *other : others)
            {
                if (other == pane)
                    continue;
                QSignalBlocker blocker(other->verticalScrollBar());
                other->verticalScrollBar()->setValue(value);
            }
            auto *gutter = static_cast<ChangeGutterWidget *>(gutter_);
            gutter->scrollOffset = value;
            gutter->update();
        });
    }
}

int SideBySideDiffWidget::scrollValue() const
{
    return oldEdit_->verticalScrollBar()->value();
}

void SideBySideDiffWidget::setScrollValue(int value)
{
    oldEdit_->verticalScrollBar()->setValue(value);
}

void SideBySideDiffWidget::setActions(Actions actions)
{
    actionMode_ = actions;
    auto *gutter = static_cast<ChangeGutterWidget *>(gutter_);
    gutter->actions = actions;
    updateGutter();
}

void SideBySideDiffWidget::setTitles(const QString &oldTitle, const QString &newTitle)
{
    oldTitleLabel_->setText(oldTitle);
    newTitleLabel_->setText(newTitle);
}

void SideBySideDiffWidget::updateCurrentRegionHighlight()
{
    // paints (or clears) the blue background of the number cells of rows [first, last]
    const auto paintRange = [](QTextEdit *numbers, int first, int last, bool on) {
        QTextDocument *doc = numbers->document();
        for (int row = first; row <= last; ++row)
        {
            const QTextBlock block = doc->findBlockByNumber(row);
            if (!block.isValid())
                break;
            QTextBlockFormat format = block.blockFormat();
            format.setBackground(on ? QColor(173, 216, 230) : QBrush());
            QTextCursor cursor(block);
            cursor.setBlockFormat(format);
        }
    };

    if (highlightedFirst_ >= 0)
    {
        paintRange(numberOldEdit_, highlightedFirst_, highlightedLast_, false);
        paintRange(numberNewEdit_, highlightedFirst_, highlightedLast_, false);
        highlightedFirst_ = highlightedLast_ = -1;
    }

    if (currentRegion_ >= 0 && currentRegion_ < regions_.size())
    {
        const auto &region = regions_[currentRegion_];
        highlightedFirst_ = region.firstEntry;
        highlightedLast_ = region.lastEntry;
        paintRange(numberOldEdit_, region.firstEntry, region.lastEntry, true);
        paintRange(numberNewEdit_, region.firstEntry, region.lastEntry, true);
    }
}

void SideBySideDiffWidget::updateGutter()
{
    auto *gutter = static_cast<ChangeGutterWidget *>(gutter_);
    gutter->regions = &regions_;
    gutter->diffLines = &diffLines_;
    gutter->document = oldEdit_->document();
    gutter->scrollOffset = scrollValue();
    gutter->update();
}

void SideBySideDiffWidget::updateChangeLabel()
{
    if (regions_.isEmpty())
    {
        changeLabel_->setText(tr("No differences"));
        previousButton_->setEnabled(false);
        nextButton_->setEnabled(false);
        return;
    }

    previousButton_->setEnabled(true);
    nextButton_->setEnabled(true);
    changeLabel_->setText(tr("Change %1 of %2").arg(currentRegion_ + 1).arg(regions_.size()));
}

void SideBySideDiffWidget::scrollToRegion(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= regions_.size())
        return;

    currentRegion_ = regionIndex;
    updateCurrentRegionHighlight();
    updateChangeLabel();

    const int row = regions_[regionIndex].firstEntry;
    const QTextBlock block = oldEdit_->document()->findBlockByNumber(row);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    oldEdit_->setTextCursor(cursor);
    oldEdit_->ensureCursorVisible();

    // center the change in the viewport
    const QRect cursorRect = oldEdit_->cursorRect();
    const int delta = cursorRect.center().y() - oldEdit_->viewport()->height() / 2;
    oldEdit_->verticalScrollBar()->setValue(oldEdit_->verticalScrollBar()->value() - delta);
}

void SideBySideDiffWidget::nextChange()
{
    if (regions_.isEmpty())
        return;
    scrollToRegion(currentRegion_ + 1 < regions_.size() ? currentRegion_ + 1 : 0);
}

void SideBySideDiffWidget::previousChange()
{
    if (regions_.isEmpty())
        return;
    scrollToRegion(currentRegion_ - 1 >= 0 ? currentRegion_ - 1 : regions_.size() - 1);
}
