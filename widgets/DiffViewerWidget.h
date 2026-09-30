#pragma once

#include <QTableWidget>

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

    explicit DiffViewerWidget(QWidget *parent = nullptr);
    ~DiffViewerWidget();

    /// Has to be called before setDiffData() to take effect
    void setRowActions(RowActions actions)
    {
        rowActions = actions;
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

private:
    RowActions rowActions = RowActions::RestoreOriginal;
    QList<DiffCalculation::LineDiffResult> currentDiffs;
};
