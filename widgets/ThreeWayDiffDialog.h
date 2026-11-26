#pragma once

#include <QDialog>
#include <QStringList>

#include "utils/ThreeWayDiff.h"

class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QVBoxLayout;
class CodeEditor;
class ThreeWayDiffWidget;
class QWidget;

/**
 * @class ThreeWayDiffDialog
 * @brief Three-way (diff3-style) comparison of a base text and two changed versions.
 *
 * The user picks three files - the common base and the two versions - or uses the
 * current editor content as the "ours" side. The result is shown side by side with
 * conflicts marked; "Save merged result" writes the automatically merged text
 * (conflicts keep both versions between <<<<<<< / ======= / >>>>>>> markers).
 */
class ThreeWayDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ThreeWayDiffDialog(CodeEditor *editor, QWidget *parent = nullptr);

private slots:
    void browseBase();
    void browseOurs();
    void browseTheirs();
    void compare();
    void saveMerged();

private:
    QStringList linesOf(const QString &filePath) const;
    void updateButtons();

    CodeEditor *editor = nullptr;

    QLineEdit *baseEdit = nullptr;
    QLineEdit *oursEdit = nullptr;
    QLineEdit *theirsEdit = nullptr;
    QCheckBox *useEditorCheck = nullptr;
    QPushButton *compareButton = nullptr;
    QPushButton *saveMergedButton = nullptr;
    QLabel *statsLabel = nullptr;
    ThreeWayDiffWidget *diffWidget = nullptr;
    QVBoxLayout *mainLayout = nullptr;

    ThreeWayDiff::Result result_;
};
