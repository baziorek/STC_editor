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
    explicit DiffViewerWidget(QWidget *parent = nullptr);
    ~DiffViewerWidget();

    void setDiffData(const QList<DiffCalculation::LineDiffResult> &diffs);

    const QList<DiffCalculation::LineDiffResult>& diffData() const
    {
        return currentDiffs;
    }

signals:
    void lineRestored(int newLineIndex, const QString &restoredText);

    void jumpToLineInEditor(int lineIndex);

private:
    QList<DiffCalculation::LineDiffResult> currentDiffs;
};
