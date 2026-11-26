#pragma once

#include <stddef.h>
#include <QDialog>
#include <QTimer>
#include <vector>
#include <QObject>
#include <QString>

#include "utils/DiffCalculation.h"
#include "SideBySideDiffWidget.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class CodeEditor;
class DiffCalculator;
class QWidget;

/**
 * @class PastedTextDiffDialog
 * @brief Compares the current editor content with a text pasted by the user.
 *
 * Typical use: a corrected version of the whole article comes back from an
 * external source (e.g. an AI assistant). The user pastes it here and sees the
 * differences from the text currently in the editor - without any external diff tool.
 *
 * The diff is shown side by side (SideBySideDiffWidget):
 * - "old" side: current editor content,
 * - "new" side: pasted text,
 * - the diff is recalculated (debounced) whenever the pasted text changes,
 * - "←" on a run of changes keeps the editor's lines (the pasted text is changed
 *   so that this difference disappears),
 * - "→" on a run of changes takes the pasted lines into the editor (one undo step),
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
    void acceptRegion(int regionIndex);
    void discardRegion(int regionIndex);

private:
    QString pastedTextMatchingEditorEnding() const;

    CodeEditor *editor = nullptr;
    QPlainTextEdit *pasteEdit = nullptr;
    QLabel *statsLabel = nullptr;
    SideBySideDiffWidget *diffWidget = nullptr;
    QPushButton *applyButton = nullptr;
    QTimer recomputeTimer;
    std::vector<DiffCalculation::DiffLine> fullDiff; // of the last finished recompute, including unchanged lines
    DiffCalculator *calculator = nullptr;
    quint64 pendingGeneration_ = 0;
    int pendingScroll_ = 0;
};
