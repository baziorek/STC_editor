#pragma once

#include <QDialog>
#include <QTimer>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class CodeEditor;
class DiffViewerWidget;

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

private:
    QString pastedTextMatchingEditorEnding() const;

    CodeEditor *editor = nullptr;
    QPlainTextEdit *pasteEdit = nullptr;
    QLabel *statsLabel = nullptr;
    DiffViewerWidget *diffWidget = nullptr;
    QPushButton *applyButton = nullptr;
    QTimer recomputeTimer;
};
