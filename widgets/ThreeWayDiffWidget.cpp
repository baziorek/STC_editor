#include "ThreeWayDiffWidget.h"

#include "ui_ThreeWayDiffWidget.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <Qt>

namespace
{

/// The background of a row on one side, by the kind of the row.
QColor backgroundFor(ThreeWayDiff::RowKind kind, int side)
{
    using ThreeWayDiff::RowKind;
    switch (kind)
    {
    case RowKind::ChangedOurs:
        return side == 1 ? QColor(206, 240, 206) : QColor();
    case RowKind::ChangedTheirs:
        return side == 2 ? QColor(189, 222, 240) : QColor();
    case RowKind::SameChange:
        return QColor(255, 236, 190);
    case RowKind::Conflict:
        return side != 0 ? QColor(255, 205, 205) : QColor();
    case RowKind::Unchanged:
        break;
    }
    return QColor();
}

} // namespace


ThreeWayDiffWidget::ThreeWayDiffWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::ThreeWayDiffWidget)
{
    ui->setupUi(this);

    baseTitleLabel_ = ui->baseTitleLabel;
    oursTitleLabel_ = ui->oursTitleLabel;
    theirsTitleLabel_ = ui->theirsTitleLabel;
    conflictLabel_ = ui->conflictLabel;
    previousButton_ = ui->previousButton;
    nextButton_ = ui->nextButton;

    baseNumbersEdit_ = ui->baseNumbersEdit;
    baseEdit_ = ui->baseEdit;
    oursNumbersEdit_ = ui->oursNumbersEdit;
    oursEdit_ = ui->oursEdit;
    theirsNumbersEdit_ = ui->theirsNumbersEdit;
    theirsEdit_ = ui->theirsEdit;

    // the panes share one fixed font and have no document margin; the numbers columns are narrow
    for (QTextEdit *edit : {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                            theirsNumbersEdit_, theirsEdit_})
    {
        edit->document()->setDocumentMargin(0);
        edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    }
    for (QTextEdit *numbers : {baseNumbersEdit_, oursNumbersEdit_, theirsNumbersEdit_})
    {
        numbers->setFixedWidth(48);
        numbers->setTextInteractionFlags(Qt::NoTextInteraction);
    }

    // the titles of the three sides are bold
    for (QLabel *label : {baseTitleLabel_, oursTitleLabel_, theirsTitleLabel_})
    {
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
    }

    syncScrollBars();

    connect(previousButton_, &QPushButton::clicked, this, &ThreeWayDiffWidget::previousConflict);
    connect(nextButton_, &QPushButton::clicked, this, &ThreeWayDiffWidget::nextConflict);
}

ThreeWayDiffWidget::~ThreeWayDiffWidget()
{
    delete ui;
}

void ThreeWayDiffWidget::setResult(const ThreeWayDiff::Result &result)
{
    result_ = result;

    QStringList baseLines, oursLines, theirsLines;
    QStringList baseNumbers, oursNumbers, theirsNumbers;
    const int count = result_.rows.size();
    baseLines.reserve(count);
    oursLines.reserve(count);
    theirsLines.reserve(count);
    baseNumbers.reserve(count);
    oursNumbers.reserve(count);
    theirsNumbers.reserve(count);

    for (const auto &row : result_.rows)
    {
        baseLines << row.base;
        oursLines << row.ours;
        theirsLines << row.theirs;
        baseNumbers << (row.baseNumber >= 0 ? QString::number(row.baseNumber + 1) : QString());
        oursNumbers << (row.oursNumber >= 0 ? QString::number(row.oursNumber + 1) : QString());
        theirsNumbers << (row.theirsNumber >= 0 ? QString::number(row.theirsNumber + 1) : QString());
    }

    fillPanes(baseLines.join('\n'), oursLines.join('\n'), theirsLines.join('\n'),
              baseNumbers.join('\n'), oursNumbers.join('\n'), theirsNumbers.join('\n'));

    applyRowFormats();

    // jump to the first conflict (or the top when there is none)
    int firstConflict = -1;
    for (int r = 0; r < result_.regions.size(); ++r)
    {
        if (result_.regions[r].kind == ThreeWayDiff::RowKind::Conflict)
        {
            firstConflict = r;
            break;
        }
    }
    currentConflict_ = firstConflict;
    if (firstConflict >= 0)
        scrollToRegion(firstConflict);
    else
        syncScrollValue(0);

    updateConflictLabel();
}

void ThreeWayDiffWidget::applyRowFormats()
{
    const auto apply = [this](QTextEdit *edit, int side) {
        QTextDocument *doc = edit->document();
        for (int row = 0; row < result_.rows.size(); ++row)
        {
            const QColor color = backgroundFor(result_.rows[row].kind, side);
            if (color.isValid())
            {
                const QTextBlock block = doc->findBlockByNumber(row);
                if (!block.isValid())
                    break;
                QTextBlockFormat blockFormat = block.blockFormat();
                blockFormat.setBackground(color);
                QTextCursor cursor(block);
                cursor.setBlockFormat(blockFormat);
            }
        }
    };

    apply(baseEdit_, 0);
    apply(oursEdit_, 1);
    apply(theirsEdit_, 2);
}

void ThreeWayDiffWidget::setPlainLines(const QStringList &baseLines, const QStringList &oursLines,
                                       const QStringList &theirsLines)
{
    result_ = ThreeWayDiff::Result();
    currentConflict_ = -1;

    QStringList baseNumbers, oursNumbers, theirsNumbers;
    for (int i = 1; i <= baseLines.size(); ++i) baseNumbers << QString::number(i);
    for (int i = 1; i <= oursLines.size(); ++i) oursNumbers << QString::number(i);
    for (int i = 1; i <= theirsLines.size(); ++i) theirsNumbers << QString::number(i);

    fillPanes(baseLines.join('\n'), oursLines.join('\n'), theirsLines.join('\n'),
              baseNumbers.join('\n'), oursNumbers.join('\n'), theirsNumbers.join('\n'));

    conflictLabel_->setText(tr("Computing diff…"));
    previousButton_->setEnabled(false);
    nextButton_->setEnabled(false);
}

void ThreeWayDiffWidget::fillPanes(const QString &baseText, const QString &oursText, const QString &theirsText,
                                   const QString &baseNumbers, const QString &oursNumbers,
                                   const QString &theirsNumbers)
{
    baseNumbersEdit_->setPlainText(baseNumbers);
    oursNumbersEdit_->setPlainText(oursNumbers);
    theirsNumbersEdit_->setPlainText(theirsNumbers);
    baseEdit_->setPlainText(baseText);
    oursEdit_->setPlainText(oursText);
    theirsEdit_->setPlainText(theirsText);

    // gray, right-aligned numbers
    for (QTextEdit *numbers : {baseNumbersEdit_, oursNumbersEdit_, theirsNumbersEdit_})
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
    syncScrollValue(0);
}

void ThreeWayDiffWidget::setTitles(const QString &baseTitle, const QString &oursTitle, const QString &theirsTitle)
{
    baseTitleLabel_->setText(baseTitle);
    oursTitleLabel_->setText(oursTitle);
    theirsTitleLabel_->setText(theirsTitle);
}

void ThreeWayDiffWidget::syncScrollBars()
{
    const auto panes = {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                        theirsNumbersEdit_, theirsEdit_};
    for (QTextEdit *pane : panes)
    {
        connect(pane->verticalScrollBar(), &QScrollBar::valueChanged, this,
                [this, pane](int) { syncScrollFrom(pane); });
    }
}

void ThreeWayDiffWidget::syncScrollFrom(QTextEdit *pane)
{
    // re-entrant call from a pane we just scrolled ourselves - its position is already right
    if (syncingScroll_)
        return;

    // sync by the row at the top of the pane that initiated the scroll, not by the raw
    // scrollbar value: a scrollbar can report a range larger than the pane can actually
    // scroll (stale QTextEdit layout), and copying that value would leave the other
    // panes' text frozen while their thumbs still move
    const int value = pane->verticalScrollBar()->value();
    const int firstRow = pane->cursorForPosition(QPoint(2, 1)).blockNumber();
    if (firstRow < 0)
        return;
    const auto blockTop = [](QTextEdit *edit, int row) {
        const QTextBlock block = edit->document()->findBlockByNumber(row);
        return block.isValid()
            ? edit->document()->documentLayout()->blockBoundingRect(block).top()
            : -1.0;
    };
    const qreal rowTop = blockTop(pane, firstRow);
    const int subLine = qMax(0, value - int(rowTop));

    const auto all = {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                      theirsNumbersEdit_, theirsEdit_};
    // do NOT block the scrollbar signals: QTextEdit scrolls its viewport from its own
    // valueChanged handler, and a blocked setValue moves the thumb while the text
    // stays put. The syncingScroll_ flag keeps our lambda from reacting to the echoes.
    syncingScroll_ = true;
    for (QTextEdit *other : all)
    {
        if (other == pane)
            continue;
        const int targetRow = qMin(firstRow, other->document()->blockCount() - 1);
        const qreal otherTop = blockTop(other, targetRow);
        if (otherTop < 0)
            continue;
        other->verticalScrollBar()->setValue(int(otherTop) + subLine);
    }
    syncingScroll_ = false;
}

void ThreeWayDiffWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scheduleScrollBarsFix();
}

void ThreeWayDiffWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    scheduleScrollBarsFix();
}

void ThreeWayDiffWidget::scheduleScrollBarsFix()
{
    // the panes reach their final height only after the layout polish - fix the ranges then
    QTimer::singleShot(0, this, [this]() {
        updateScrollBarRanges();
        // clamping during the range fix may have moved the panes apart - re-sync them
        syncScrollValue(baseEdit_->verticalScrollBar()->value());
        syncScrollFrom(baseEdit_);
    });
}

void ThreeWayDiffWidget::syncScrollValue(int value)
{
    // the signals must reach QTextEdit's own handlers (they do the actual scrolling) -
    // syncingScroll_ only keeps our sync lambda from reacting to the echoes
    syncingScroll_ = true;
    for (QTextEdit *edit : {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                            theirsNumbersEdit_, theirsEdit_})
        edit->verticalScrollBar()->setValue(value);
    syncingScroll_ = false;
}

void ThreeWayDiffWidget::updateScrollBarRanges()
{
    for (QTextEdit *edit : {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                            theirsNumbersEdit_, theirsEdit_})
    {
        const int documentHeight =
            int(edit->document()->documentLayout()->documentSize().height());
        const int viewportHeight = edit->viewport()->height();
        if (viewportHeight <= 0)
            continue; // hidden: the viewport has no meaningful height yet
        const int maxScroll = qMax(0, documentHeight - viewportHeight);
        QScrollBar *bar = edit->verticalScrollBar();
        if (bar->maximum() != maxScroll)
            bar->setMaximum(maxScroll);
    }
}

void ThreeWayDiffWidget::updateConflictLabel()
{
    QVector<int> conflicts;
    for (int r = 0; r < result_.regions.size(); ++r)
    {
        if (result_.regions[r].kind == ThreeWayDiff::RowKind::Conflict)
            conflicts.append(r);
    }

    if (conflicts.isEmpty())
    {
        conflictLabel_->setText(tr("No conflicts"));
        previousButton_->setEnabled(false);
        nextButton_->setEnabled(false);
        return;
    }

    previousButton_->setEnabled(true);
    nextButton_->setEnabled(true);
    const int pos = conflicts.indexOf(currentConflict_);
    conflictLabel_->setText(tr("Conflict %1 of %2").arg(pos + 1).arg(conflicts.size()));
}

void ThreeWayDiffWidget::scrollToRegion(int regionIndex)
{
    if (regionIndex < 0 || regionIndex >= result_.regions.size())
        return;

    const int row = result_.regions[regionIndex].firstRow;
    const QTextBlock block = baseEdit_->document()->findBlockByNumber(row);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    baseEdit_->setTextCursor(cursor);
    baseEdit_->ensureCursorVisible();

    // center the region in the viewport
    const QRect cursorRect = baseEdit_->cursorRect();
    const int delta = cursorRect.center().y() - baseEdit_->viewport()->height() / 2;
    baseEdit_->verticalScrollBar()->setValue(baseEdit_->verticalScrollBar()->value() - delta);
}

void ThreeWayDiffWidget::nextConflict()
{
    QVector<int> conflicts;
    for (int r = 0; r < result_.regions.size(); ++r)
    {
        if (result_.regions[r].kind == ThreeWayDiff::RowKind::Conflict)
            conflicts.append(r);
    }
    if (conflicts.isEmpty())
        return;

    const int pos = conflicts.indexOf(currentConflict_);
    const int next = (pos + 1) % conflicts.size();
    currentConflict_ = conflicts[next];
    scrollToRegion(currentConflict_);
    updateConflictLabel();
}

void ThreeWayDiffWidget::previousConflict()
{
    QVector<int> conflicts;
    for (int r = 0; r < result_.regions.size(); ++r)
    {
        if (result_.regions[r].kind == ThreeWayDiff::RowKind::Conflict)
            conflicts.append(r);
    }
    if (conflicts.isEmpty())
        return;

    const int pos = conflicts.indexOf(currentConflict_);
    const int prev = (pos <= 0 ? conflicts.size() - 1 : pos - 1);
    currentConflict_ = conflicts[prev];
    scrollToRegion(currentConflict_);
    updateConflictLabel();
}
