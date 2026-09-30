#pragma once

#include <QList>
#include <QTableWidget>
#include <QTimer>
#include <QVector>

namespace DiffCalculation
{
struct LineDiffResult;
}

class DiffViewerWidget : public QTableWidget
{
    Q_OBJECT

public:
    /// What the buttons in the last column do
    enum class RowActions
    {
        RestoreOriginal,       ///< one "↩" button per row -> lineRestored() (default; used by unsaved-changes / backup dialogs)
        AcceptOrDiscardChange  ///< "←" / "→" buttons per row -> changeAccepted() / changeDiscarded() (used when comparing with pasted text)
    };

    /// A run of consecutive changed rows that can be moved as a whole (AcceptOrDiscardChange mode only).
    /// A header row with the title and two buttons is shown right above the rows of the block.
    struct Block
    {
        int firstRow = 0;   ///< index into diffData()
        int lastRow = 0;    ///< index into diffData(), inclusive
        QString title;
    };

    explicit DiffViewerWidget(QWidget *parent = nullptr);
    ~DiffViewerWidget();

    /// Has to be called before setDiffData() to take effect
    void setRowActions(RowActions actions)
    {
        rowActions = actions;
    }

    /// Has to be called before setDiffData() to take effect; blocks are only shown in AcceptOrDiscardChange mode
    void setBlocks(const QList<Block> &newBlocks)
    {
        blocks = newBlocks;
    }

    void setDiffData(const QList<DiffCalculation::LineDiffResult> &diffs);

    const QList<DiffCalculation::LineDiffResult>& diffData() const
    {
        return currentDiffs;
    }

signals:
    void lineRestored(int newLineIndex, const QString &restoredText);

    void jumpToLineInEditor(int lineIndex);

    /// Only in RowActions::AcceptOrDiscardChange mode; `row` is an index into diffData()
    void changeAccepted(int row);
    void changeDiscarded(int row);

    /// Only in RowActions::AcceptOrDiscardChange mode; `blockIndex` is an index into the list given to setBlocks()
    void blockAccepted(int blockIndex);
    void blockDiscarded(int blockIndex);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void scheduleRowHeights();
    void updateRowHeights();

    RowActions rowActions = RowActions::RestoreOriginal;
    QList<DiffCalculation::LineDiffResult> currentDiffs;
    QList<Block> blocks;

    QVector<int> tableRowToDiff; // -1 for a block header row
    QTimer rowHeightsTimer;
    int lastColumn1Width = -1;
    int lastColumn3Width = -1;
    bool rowHeightsDirty = false;
};
