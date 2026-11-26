#pragma once

#include <QDialog>

class CodeEditor;
class DiffViewerWidget;
class QLabel;

/**
 * @class FileDiffDialog
 * @brief Shows the line diff of the current editor content against another file, read only.
 *
 * The "old" side is the text of the editor, the "new" side is the content of the chosen file.
 * Nothing can be changed from here: it is for looking at the differences ("Compare with file…" in the Edit menu).
 */
class FileDiffDialog : public QDialog
{
    Q_OBJECT

public:
    FileDiffDialog(CodeEditor* editor, const QString& otherFilePath, QWidget* parent = nullptr);

private:
    CodeEditor* editor = nullptr;
    QString otherFilePath;
    QLabel* statsLabel = nullptr;
    DiffViewerWidget* diffWidget = nullptr;
};
