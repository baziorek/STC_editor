#pragma once

#include <QDialog>
#include <QStringList>

class CodeEditor;
class QLabel;
class SideBySideDiffWidget;

/**
 * @class FileDiffDialog
 * @brief Shows the line diff of two texts, read only.
 *
 * Used by "Compare with file…" (the editor content against a file) and "Compare files…"
 * (two files from the disk). Nothing can be changed from here: it is for looking at the differences.
 */
class FileDiffDialog : public QDialog
{
    Q_OBJECT

public:
    /// The "old" side is the text of the editor, the "new" side is the content of the chosen file.
    FileDiffDialog(CodeEditor* editor, const QString& otherFilePath, QWidget* parent = nullptr);
    /// Both sides are files from the disk; `oldFilePath` is shown on the left, `newFilePath` on the right.
    FileDiffDialog(const QString& oldFilePath, const QString& newFilePath, QWidget* parent = nullptr);

private:
    void init(const QStringList& oldLines, const QStringList& newLines, const QString& title);

    CodeEditor* editor = nullptr;
    QLabel* statsLabel = nullptr;
    SideBySideDiffWidget* diffWidget = nullptr;
};
