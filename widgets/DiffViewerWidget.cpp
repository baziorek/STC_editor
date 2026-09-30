#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTextBrowser>
#include <QTextDocument>
#include <QLabel>
#include <algorithm>
#include <cmath>
#include "DiffViewerWidget.h"
#include "utils/DiffCalculation.h"


namespace
{
using namespace DiffCalculation;

constexpr int cellPadding = 4;      // text distance from the cell border, the same for both sides
constexpr int buttonRowHeight = 30; // a row always has room for the 24 px buttons
constexpr int blockHeaderHeight = 36;

/// Equal text as is; the fragments of `changeType` (deleted on the old side, inserted on the new side) in `changeStyle`
QString styledHtml(const QList<LineDiffFragment> &fragments, FragmentType changeType, const QString &changeStyle)
{
    QString html;
    for (const auto &frag : fragments)
    {
        const QString escaped = frag.text.toHtmlEscaped();
        if (frag.type == FragmentType::Equal)
            html += escaped;
        else if (frag.type == changeType)
            html += "<span style='" + changeStyle + "'>" + escaped + "</span>";
    }
    return html;
}

/// Unicode code points of the line - helps to spot invisible differences (non-breaking spaces, ...)
QString codePointsTooltip(const QList<LineDiffFragment> &fragments, FragmentType changeType, const QString &changeColor)
{
    QString tooltip;
    for (const auto &frag : fragments)
    {
        const QString color = (frag.type == changeType)         ? changeColor
                              : (frag.type == FragmentType::Equal) ? "gray"
                                                                   : "black"; // fallback
        for (const auto &ch : frag.text)
        {
            tooltip += QString("<span style='color:%1'>U+%2</span> ")
                           .arg(color)
                           .arg(QString::number(ch.unicode(), 16).toUpper().rightJustified(4, '0'));
        }
    }
    return tooltip.trimmed();
}

QPushButton *makeSquareButton(QWidget *parent, const QString &text, const QString &tooltip)
{
    auto *button = new QPushButton(text, parent);
    button->setToolTip(tooltip);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setFixedSize(24, 24);
    return button;
}
} // namespace


DiffViewerWidget::DiffViewerWidget(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(5);
    setHorizontalHeaderLabels({"Old #", "Old Line", "New #", "New Line", ""});

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Old #
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);          // Old Line
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // New #
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);          // New Line
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Restore button

    verticalHeader()->setVisible(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed); // heights are set by updateRowHeights()
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    horizontalHeaderItem(0)->setToolTip("Line number of old file");
    horizontalHeaderItem(1)->setToolTip("Line content of old file");
    horizontalHeaderItem(2)->setToolTip("Line number of new file");
    horizontalHeaderItem(3)->setToolTip("Line content of new file");
    horizontalHeaderItem(4)->setToolTip("Buttons to restore to original");

    // The text wraps at the column width, so the row heights follow the column widths
    rowHeightsTimer.setSingleShot(true);
    rowHeightsTimer.setInterval(0);
    connect(&rowHeightsTimer, &QTimer::timeout, this, &DiffViewerWidget::updateRowHeights);
    connect(horizontalHeader(), &QHeaderView::sectionResized, this, &DiffViewerWidget::scheduleRowHeights);
}

DiffViewerWidget::~DiffViewerWidget() = default;

void DiffViewerWidget::showEvent(QShowEvent *event)
{
    QTableWidget::showEvent(event);
    scheduleRowHeights();
}

void DiffViewerWidget::scheduleRowHeights()
{
    rowHeightsTimer.start();
}

/// Row height = the higher of the two texts (wrapped at the current column width); both cells fill the whole row,
/// so the old and the new side always have the same height.
void DiffViewerWidget::updateRowHeights()
{
    const int width1 = columnWidth(1);
    const int width3 = columnWidth(3);

    if (!rowHeightsDirty && width1 == lastColumn1Width && width3 == lastColumn3Width)
        return;

    lastColumn1Width = width1;
    lastColumn3Width = width3;
    rowHeightsDirty = false;

    for (int row = 0; row < rowCount() && row < tableRowToDiff.size(); ++row)
    {
        if (tableRowToDiff[row] < 0) // block header
        {
            setRowHeight(row, blockHeaderHeight);
            continue;
        }

        int height = buttonRowHeight;

        if (auto *oldLabel = qobject_cast<QLabel *>(cellWidget(row, 1)))
            height = std::max(height, oldLabel->heightForWidth(std::max(10, width1 - 1)));

        if (auto *newBrowser = qobject_cast<QTextBrowser *>(cellWidget(row, 3)))
        {
            QTextDocument *doc = newBrowser->document();
            doc->setTextWidth(std::max(10, width3 - 1));
            height = std::max(height, static_cast<int>(std::ceil(doc->size().height())));
        }

        setRowHeight(row, height + 1); // + the grid line
    }
}

void DiffViewerWidget::setDiffData(const QList<DiffCalculation::LineDiffResult> &diffs)
{
    const bool showBlocks = (rowActions == RowActions::AcceptOrDiscardChange);

    // Table rows: every diff is one row; a block additionally gets a header row above its first diff
    QVector<int> blockStartingAt(diffs.size(), -1);
    int headerCount = 0;
    if (showBlocks)
    {
        for (int b = 0; b < blocks.size(); ++b)
        {
            const auto &block = blocks[b];
            if (block.firstRow >= 0 && block.lastRow > block.firstRow && block.lastRow < diffs.size())
            {
                blockStartingAt[block.firstRow] = b;
                ++headerCount;
            }
        }
    }

    setRowCount(0); // also deletes the cell widgets of the previous data
    clearSpans();
    setRowCount(diffs.size() + headerCount);

    tableRowToDiff.clear();
    tableRowToDiff.reserve(diffs.size() + headerCount);

    int row = 0;
    for (int diffIndex = 0; diffIndex < diffs.size(); ++diffIndex)
    {
        if (blockStartingAt[diffIndex] >= 0)
        {
            const int blockIndex = blockStartingAt[diffIndex];

            auto *header = new QWidget(this);
            header->setObjectName("blockHeader");
            header->setAttribute(Qt::WA_StyledBackground, true);
            header->setStyleSheet("#blockHeader { background: #dbe5f1; }");

            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(6, 2, 6, 2);

            auto *title = new QLabel(blocks[blockIndex].title, header);
            QFont bold = title->font();
            bold.setBold(true);
            title->setFont(bold);
            headerLayout->addWidget(title, 1);

            const int rowsInBlock = blocks[blockIndex].lastRow - blocks[blockIndex].firstRow + 1;
            auto *acceptBlock = new QPushButton(tr("← Apply block"), header);
            acceptBlock->setToolTip(tr("Apply all %1 changes of this block to the editor at once").arg(rowsInBlock));
            auto *discardBlock = new QPushButton(tr("→ Discard block"), header);
            discardBlock->setToolTip(tr("Discard all %1 changes of this block (keep the editor's lines)").arg(rowsInBlock));
            headerLayout->addWidget(acceptBlock);
            headerLayout->addWidget(discardBlock);

            connect(acceptBlock, &QPushButton::clicked, this, [this, blockIndex]() { emit blockAccepted(blockIndex); });
            connect(discardBlock, &QPushButton::clicked, this, [this, blockIndex]() { emit blockDiscarded(blockIndex); });

            setSpan(row, 0, 1, columnCount());
            setCellWidget(row, 0, header);
            tableRowToDiff.append(-1);
            ++row;
        }

        const auto &diff = diffs[diffIndex];

        // Old line number
        auto *oldLineItem = new QTableWidgetItem();
        if (diff.oldLineIndex >= 0)
            oldLineItem->setText(QString::number(diff.oldLineIndex + 1));
        oldLineItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        setItem(row, 0, oldLineItem);

        // Old line text (nothing to show for an added line)
        if (!diff.oldFragments.isEmpty())
        {
            auto *oldLabel = new QLabel(styledHtml(diff.oldFragments, FragmentType::Delete, "color:red;text-decoration:line-through"));
            oldLabel->setTextFormat(Qt::RichText);
            oldLabel->setWordWrap(true);
            oldLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            oldLabel->setMargin(cellPadding);
            oldLabel->setToolTip(codePointsTooltip(diff.oldFragments, FragmentType::Delete, "red"));
            setCellWidget(row, 1, oldLabel);
        }

        // New line number
        auto *newLineItem = new QTableWidgetItem();
        if (diff.newLineIndex >= 0)
            newLineItem->setText(QString::number(diff.newLineIndex + 1));
        newLineItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        setItem(row, 2, newLineItem);

        // New line text (nothing to show for a removed line)
        if (!diff.newFragments.isEmpty())
        {
            auto *newEdit = new QTextBrowser(this);
            newEdit->setFrameShape(QFrame::NoFrame); // no frame, no scrollbars: the row is as high as the text
            newEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            newEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            newEdit->document()->setDocumentMargin(cellPadding);
            newEdit->setHtml(styledHtml(diff.newFragments, FragmentType::Insert, "color:green"));
            newEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            newEdit->setToolTip(codePointsTooltip(diff.newFragments, FragmentType::Insert, "green"));
            newEdit->setCursor(Qt::PointingHandCursor);
            connect(newEdit, &QTextBrowser::cursorPositionChanged, this, [this, diff]() {
                if (diff.newLineIndex >= 0)
                    emit jumpToLineInEditor(diff.newLineIndex);
            });
            setCellWidget(row, 3, newEdit);
        }

        if (rowActions == RowActions::AcceptOrDiscardChange)
        {
            // Meld-like arrows: "←" takes the right-hand (new) line into the left-hand side,
            // "→" keeps the left-hand (old) line, i.e. drops this change from the diff
            auto *box = new QWidget(this);
            auto *boxLayout = new QHBoxLayout(box);
            boxLayout->setContentsMargins(2, 0, 2, 0);
            boxLayout->setSpacing(2);
            boxLayout->setAlignment(Qt::AlignTop);

            auto *acceptBtn = makeSquareButton(box, "←", tr("Apply this change to the editor (take the right-hand line)"));
            auto *discardBtn = makeSquareButton(box, "→", tr("Discard this change (keep the editor's line)"));
            boxLayout->addWidget(acceptBtn);
            boxLayout->addWidget(discardBtn);
            connect(acceptBtn, &QPushButton::clicked, this, [this, diffIndex]() { emit changeAccepted(diffIndex); });
            connect(discardBtn, &QPushButton::clicked, this, [this, diffIndex]() { emit changeDiscarded(diffIndex); });

            setCellWidget(row, 4, box);
        }
        else
        {
            // Restore button
            QPushButton *restoreBtn = makeSquareButton(this, "↩", "Restore original line");
            connect(restoreBtn, &QPushButton::clicked, this, [this, diff]() {
                int lineIndexToRestore = (diff.newLineIndex >= 0) ? diff.newLineIndex : diff.oldLineIndex;
                emit lineRestored(lineIndexToRestore, diff.oldText());
            });
            setCellWidget(row, 4, restoreBtn);
        }

        if (diff.oldLineIndex == -1) // added new line
        {
            for (int col = 0; col < columnCount(); ++col)
            {
                auto* i = item(row, col);
                if (i)
                {
                    i->setBackground(Qt::green);
                    i->setForeground(Qt::black);
                }
            }
        }
        else if (diff.newLineIndex == -1) // removed line
        {
            for (int col = 0; col < columnCount(); ++col)
            {
                auto* i = item(row, col);
                if (i)
                {
                    i->setBackground(Qt::red);
                    i->setForeground(Qt::black);
                }
            }
        }

        tableRowToDiff.append(diffIndex);
        ++row;
    }

    currentDiffs = diffs;

    rowHeightsDirty = true;
    updateRowHeights(); // right away for what is known now ...
    scheduleRowHeights(); // ... and again once the layout has settled
}
