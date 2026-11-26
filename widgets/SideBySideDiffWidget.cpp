#include "SideBySideDiffWidget.h"

#include "ChangeGutterWidget.h"
#include "ui_SideBySideDiffWidget.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <Qt>

SideBySideDiffWidget::SideBySideDiffWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::SideBySideDiffWidget)
{
    ui->setupUi(this);

    oldTitleLabel_ = ui->oldTitleLabel;
    newTitleLabel_ = ui->newTitleLabel;
    changeLabel_ = ui->changeLabel;
    previousButton_ = ui->previousButton;
    nextButton_ = ui->nextButton;

    numberOldEdit_ = ui->numberOldEdit;
    oldEdit_ = ui->oldEdit;
    gutter_ = ui->gutter;
    numberNewEdit_ = ui->numberNewEdit;
    newEdit_ = ui->newEdit;

    // the panes share one fixed font and have no document margin
    for (QTextEdit *edit : {numberOldEdit_, oldEdit_, numberNewEdit_, newEdit_})
    {
        edit->document()->setDocumentMargin(0);
        edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    }

    // the titles of the two sides are bold
    for (QLabel *label : {oldTitleLabel_, newTitleLabel_})
    {
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
    }

    auto *gutter = ui->gutter;
    gutter->actions = actionMode_;
    gutter->onTakeLeft = [this](int region) { emit regionDiscarded(region); };
    gutter->onTakeRight = [this](int region) { emit regionAccepted(region); };

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

SideBySideDiffWidget::~SideBySideDiffWidget()
{
    delete ui;
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

    updateScrollBarRanges();
    scheduleScrollBarsFix();
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
    // set every pane directly: if oldEdit_ already sits at `value` its scrollbar emits nothing
    // and the valueChanged-based sync would leave the other panes behind
    for (QTextEdit *edit : {numberOldEdit_, oldEdit_, numberNewEdit_, newEdit_})
    {
        QSignalBlocker blocker(edit->verticalScrollBar());
        edit->verticalScrollBar()->setValue(value);
    }
    auto *gutter = static_cast<ChangeGutterWidget *>(gutter_);
    gutter->scrollOffset = scrollValue();
    gutter->update();
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

void SideBySideDiffWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scheduleScrollBarsFix();
}

void SideBySideDiffWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    scheduleScrollBarsFix();
}

void SideBySideDiffWidget::scheduleScrollBarsFix()
{
    // the panes reach their final height only after the layout polish - fix the ranges then
    QTimer::singleShot(0, this, [this]() {
        updateScrollBarRanges();
        // clamping during the range fix may have moved the panes apart - re-sync them
        setScrollValue(scrollValue());
    });
}

void SideBySideDiffWidget::updateScrollBarRanges()
{
    for (QTextEdit *edit : {numberOldEdit_, oldEdit_, numberNewEdit_, newEdit_})
    {
        const int documentHeight =
            int(edit->document()->documentLayout()->documentSize().height());
        const int viewportHeight = edit->viewport()->height();
        if (viewportHeight <= 0)
            continue; // hidden: the viewport has no meaningful height yet
        const int maxScroll = qMax(0, documentHeight - viewportHeight);
        QScrollBar *bar = edit->verticalScrollBar();
        if (bar->maximum() != maxScroll)
        {
            QSignalBlocker blocker(bar);
            bar->setMaximum(maxScroll);
        }
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
