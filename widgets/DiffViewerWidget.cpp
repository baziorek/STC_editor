#include <QPushButton>
#include <QHeaderView>
#include <QTextBrowser>
#include <QLabel>
#include "DiffViewerWidget.h"
#include "utils/diffcalculation.h"


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
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    horizontalHeaderItem(0)->setToolTip("Line number of old file");
    horizontalHeaderItem(1)->setToolTip("Line content of old file");
    horizontalHeaderItem(2)->setToolTip("Line number of new file");
    horizontalHeaderItem(3)->setToolTip("Line content of new file");
    horizontalHeaderItem(4)->setToolTip("Buttons to restore to original");
}

void DiffViewerWidget::setDiffData(const QList<DiffCalculation::LineDiffResult> &diffs)
{
    using namespace DiffCalculation;

    setRowCount(diffs.size());

    for (int row = 0; row < diffs.size(); ++row)
    {
        const auto &diff = diffs[row];

        // Old line number
        auto *oldLineItem = new QTableWidgetItem();
        if (diff.oldLineIndex >= 0)
            oldLineItem->setText(QString::number(diff.oldLineIndex + 1));
        setItem(row, 0, oldLineItem);

        // Old line text
        QString oldStyled;
        for (const auto &frag : diff.oldFragments)
        {
            QString escaped = frag.text.toHtmlEscaped();
            switch (frag.type)
            {
            case FragmentType::Equal:
                oldStyled += escaped;
                break;
            case FragmentType::Delete:
                oldStyled += "<span style='color:red;text-decoration:line-through'>" + escaped + "</span>";
                break;
            default:
                break;
            }
        }
        auto *oldLabel = new QLabel(oldStyled);
        oldLabel->setTextFormat(Qt::RichText);
        oldLabel->setWordWrap(true);
        setCellWidget(row, 1, oldLabel);

        QString oldTooltip;
        for (const auto &frag : diff.oldFragments)
        {
            const QString color = (frag.type == FragmentType::Delete) ? "red"
                                  : (frag.type == FragmentType::Equal) ? "gray"
                                                                       : "black";  // fallback
            for (const auto &ch : frag.text)
            {
                oldTooltip += QString("<span style='color:%1'>U+%2</span> ")
                .arg(color)
                    .arg(QString::number(ch.unicode(), 16).toUpper().rightJustified(4, '0'));
            }
        }
        oldLabel->setToolTip(oldTooltip.trimmed());


        // New line number
        auto *newLineItem = new QTableWidgetItem();
        if (diff.newLineIndex >= 0)
            newLineItem->setText(QString::number(diff.newLineIndex + 1));
        setItem(row, 2, newLineItem);

        // New editable line
        QString newStyled;
        for (const auto &frag : diff.newFragments)
        {
            QString escaped = frag.text.toHtmlEscaped();
            switch (frag.type)
            {
            case FragmentType::Equal:
                newStyled += escaped;
                break;
            case FragmentType::Insert:
                newStyled += "<span style='color:green'>" + escaped + "</span>";
                break;
            default:
                break;
            }
        }

        auto *newEdit = new QTextBrowser(this);
        newEdit->setHtml(newStyled);
        newEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setCellWidget(row, 3, newEdit);

        QString newTooltip;
        for (const auto &frag : diff.newFragments)
        {
            const QString color = (frag.type == FragmentType::Insert) ? "green"
                                  : (frag.type == FragmentType::Equal) ? "gray"
                                                                       : "black";  // fallback
            for (const auto &ch : frag.text)
            {
                newTooltip += QString("<span style='color:%1'>U+%2</span> ")
                .arg(color)
                    .arg(QString::number(ch.unicode(), 16).toUpper().rightJustified(4, '0'));
            }
        }
        newEdit->setToolTip(newTooltip.trimmed());

        newEdit->setCursor(Qt::PointingHandCursor);
        connect(newEdit, &QTextBrowser::cursorPositionChanged, this, [this, diff]() {
            if (diff.newLineIndex >= 0)
                emit jumpToLineInEditor(diff.newLineIndex);
        });

        // Restore button
        QPushButton *restoreBtn = new QPushButton("↩", this);
        restoreBtn->setToolTip("Restore original line");
        connect(restoreBtn, &QPushButton::clicked, this, [this, row, diff]() {
            emit lineRestored(diff.newLineIndex, diff.oldText());
        });
        restoreBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        restoreBtn->setFixedSize(24, 24);
        setCellWidget(row, 4, restoreBtn);

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
    }

    resizeRowsToContents();
}
