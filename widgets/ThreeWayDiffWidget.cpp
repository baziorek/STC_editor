#include "ThreeWayDiffWidget.h"

#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace
{

QTextEdit *makePane(QWidget *parent, int width = 0)
{
    auto *edit = new QTextEdit(parent);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QTextEdit::NoWrap);
    edit->setFrameShape(QFrame::NoFrame);
    edit->document()->setDocumentMargin(0);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    if (width > 0)
        edit->setFixedWidth(width);
    return edit;
}

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


ThreeWayDiffWidget::ThreeWayDiffWidget(QWidget *parent) : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- header ---
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(6, 4, 6, 4);

    previousButton_ = new QPushButton(tr("◀ Prev conflict"), this);
    nextButton_ = new QPushButton(tr("Next conflict ▶"), this);
    conflictLabel_ = new QLabel(this);

    headerLayout->addWidget(previousButton_);
    headerLayout->addWidget(nextButton_);
    headerLayout->addWidget(conflictLabel_);
    headerLayout->addStretch();

    baseTitleLabel_ = new QLabel(this);
    baseTitleLabel_->setStyleSheet("font-weight: bold");
    headerLayout->addWidget(baseTitleLabel_);
    headerLayout->addSpacing(16);
    oursTitleLabel_ = new QLabel(this);
    oursTitleLabel_->setStyleSheet("font-weight: bold");
    headerLayout->addWidget(oursTitleLabel_);
    headerLayout->addSpacing(16);
    theirsTitleLabel_ = new QLabel(this);
    theirsTitleLabel_->setStyleSheet("font-weight: bold");
    headerLayout->addWidget(theirsTitleLabel_);

    mainLayout->addLayout(headerLayout);

    // --- panes: numbers + text for each of the three sides ---
    auto *panesLayout = new QHBoxLayout();
    panesLayout->setContentsMargins(0, 0, 0, 0);
    panesLayout->setSpacing(0);

    baseNumbersEdit_ = makePane(this, 48);
    baseNumbersEdit_->setTextInteractionFlags(Qt::NoTextInteraction);
    baseEdit_ = makePane(this);

    oursNumbersEdit_ = makePane(this, 48);
    oursNumbersEdit_->setTextInteractionFlags(Qt::NoTextInteraction);
    oursEdit_ = makePane(this);

    theirsNumbersEdit_ = makePane(this, 48);
    theirsNumbersEdit_->setTextInteractionFlags(Qt::NoTextInteraction);
    theirsEdit_ = makePane(this);

    panesLayout->addWidget(baseNumbersEdit_);
    panesLayout->addWidget(baseEdit_, 1);
    panesLayout->addWidget(oursNumbersEdit_);
    panesLayout->addWidget(oursEdit_, 1);
    panesLayout->addWidget(theirsNumbersEdit_);
    panesLayout->addWidget(theirsEdit_, 1);

    mainLayout->addLayout(panesLayout, 1);

    syncScrollBars();

    connect(previousButton_, &QPushButton::clicked, this, &ThreeWayDiffWidget::previousConflict);
    connect(nextButton_, &QPushButton::clicked, this, &ThreeWayDiffWidget::nextConflict);
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

    baseNumbersEdit_->setPlainText(baseNumbers.join('\n'));
    oursNumbersEdit_->setPlainText(oursNumbers.join('\n'));
    theirsNumbersEdit_->setPlainText(theirsNumbers.join('\n'));
    baseEdit_->setPlainText(baseLines.join('\n'));
    oursEdit_->setPlainText(oursLines.join('\n'));
    theirsEdit_->setPlainText(theirsLines.join('\n'));

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
        baseEdit_->verticalScrollBar()->setValue(0);

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
        connect(pane->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, pane](int value) {
            const auto all = {baseNumbersEdit_, baseEdit_, oursNumbersEdit_, oursEdit_,
                              theirsNumbersEdit_, theirsEdit_};
            for (QTextEdit *other : all)
            {
                if (other == pane)
                    continue;
                QSignalBlocker blocker(other->verticalScrollBar());
                other->verticalScrollBar()->setValue(value);
            }
        });
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
