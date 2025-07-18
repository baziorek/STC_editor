#pragma once

#include <QTableWidget>

namespace DiffCalculation
{
class LineDiffResult;
}

class DiffViewerWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit DiffViewerWidget(QWidget *parent = nullptr);

    void setDiffData(const QList<DiffCalculation::LineDiffResult> &diffs);

signals:
    void lineRestored(int newLineIndex, const QString &restoredText);

    void jumpToLineInEditor(int lineIndex);
};
