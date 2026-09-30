#pragma once

#include <QDialog>
#include <QTimer>
#include <vector>
#include "utils/DiffCalculation.h"
#include "DiffViewerWidget.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class CodeEditor;

/**
 * @class PastedTextDiffDialog
 * @brief Compares the current editor content with a text pasted by the user.
 *
 * Typical use: a corrected version of the whole article comes back from an
 * external source (e.g. an AI assistant). The user pastes it here and sees, line by line,
 * what differs from the text currently in the editor - without any external diff tool.
 *
 * - "old" side of the diff: current editor content,
 * - "new" side of the diff: pasted text,
 * - the diff is recalculated (debounced) whenever the pasted text changes,
 * - a run of consecutive changed lines gets a header with the line ranges and buttons that move the whole run,
 * - "←" on a row takes the pasted version of that line into the editor (one undo step per click),
 * - "→" on a row keeps the editor's line: the pasted text is changed so that this difference disappears,
 * - "Apply" replaces the whole editor content with the pasted text as a single undo step (Ctrl+Z).
 */
class PastedTextDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PastedTextDiffDialog(CodeEditor *editor, QWidget *parent = nullptr);

private slots:
    void pasteFromClipboard();
    void recomputeDiff();
    void applyPastedText();
    void acceptChange(int row);
    void discardChange(int row);
    void acceptBlock(int blockIndex);
    void discardBlock(int blockIndex);

private:
    /// A run of consecutive changed lines, all of it can be moved with one click. Indexes are into fullDiff / line numbers.
    struct Block
    {
        size_t firstEntry = 0, lastEntry = 0;                 // fullDiff entries of the run
        int oldFirst = -1, oldCount = 0, insertAfterOld = -1; // its lines in the editor (insertAfterOld: where they would go when oldCount == 0)
        int newFirst = -1, newCount = 0, insertAfterNew = -1; // its lines in the pasted text
    };

    QList<DiffViewerWidget::Block> findBlocks(const QList<DiffCalculation::LineDiffResult> &rows);
    const DiffCalculation::DiffLine *findFullDiffLine(int oldIndex, int newIndex, int *position = nullptr) const;

    QString pastedTextMatchingEditorEnding() const;

    CodeEditor *editor = nullptr;
    QPlainTextEdit *pasteEdit = nullptr;
    QLabel *statsLabel = nullptr;
    DiffViewerWidget *diffWidget = nullptr;
    QPushButton *applyButton = nullptr;
    QTimer recomputeTimer;
    std::vector<DiffCalculation::DiffLine> fullDiff; // of the last recomputeDiff(), including unchanged lines
    std::vector<Block> blocks;                       // of the last recomputeDiff()
};
