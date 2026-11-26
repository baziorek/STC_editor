#pragma once

#include <QList>
#include <QVector>
#include <QWidget>

#include "utils/DiffCalculation.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextEdit;

/**
 * @class SideBySideDiffWidget
 * @brief A meld-like view of a line diff: both documents side by side, fully aligned.
 *
 * The aligned diff (DiffCalculation::computeDiff) is turned into two documents with the same
 * number of lines - a line missing on one side becomes an empty placeholder - so the two text
 * panes always scroll together. Changed lines are colored (red removed / green added /
 * yellow modified) and the changed characters inside modified lines are highlighted.
 * A narrow gutter between the panes holds "take left" / "take right" buttons over every run
 * of consecutive changes, and the header offers previous/next change navigation.
 *
 * The widget only reports what the user chose; applying the choice is up to the dialog
 * (the signals carry the index into changeRegions(), whose entries are indices into
 * diffLines()).
 */
class SideBySideDiffWidget : public QWidget
{
    Q_OBJECT

public:
    /// What the gutter buttons do
    enum class Actions
    {
        None,           ///< no buttons: the diff is only shown (file comparison)
        AcceptDiscard,  ///< "take left" / "take right" per change run -> regionDiscarded() / regionAccepted()
        RestoreOnly     ///< a single "restore" button per change run -> regionDiscarded() (review of changes)
    };

    /// A run of consecutive changed lines of the aligned diff; firstEntry/lastEntry are
    /// indices into diffLines() (and into the row numbers of the panes).
    struct ChangeRegion
    {
        int firstEntry = 0;
        int lastEntry = 0;
    };

    explicit SideBySideDiffWidget(QWidget *parent = nullptr);

    /// Rebuilds the whole view from the aligned diff (see DiffCalculation::computeDiff).
    void setDiffLines(const std::vector<DiffCalculation::DiffLine> &diffLines);

    void setActions(Actions actions);
    void setTitles(const QString &oldTitle, const QString &newTitle);

    const std::vector<DiffCalculation::DiffLine> &diffLines() const { return diffLines_; }
    const QList<DiffCalculation::LineDiffResult> &lineDiffs() const { return lineDiffs_; }
    const QVector<ChangeRegion> &changeRegions() const { return regions_; }

    /// The scroll offset shared by all panes (for keeping the place after a rebuild)
    int scrollValue() const;
    void setScrollValue(int value);

signals:
    /// Take the right-hand (new) version of the change run `regionIndex` into the left side
    void regionAccepted(int regionIndex);
    /// Take the left-hand (old) version of the change run `regionIndex` (or restore it, in RestoreOnly mode)
    void regionDiscarded(int regionIndex);
    /// A click in the right-hand pane asked to show that line of the "new" document in the editor
    void jumpToLineInEditor(int lineIndex);

private:
    void buildPanes();
    void applyLineFormats(QTextEdit *edit, bool oldSide);
    void syncScrollBars();
    void updateChangeLabel();
    void scrollToRegion(int regionIndex);
    void nextChange();
    void previousChange();
    void updateGutter();

    std::vector<DiffCalculation::DiffLine> diffLines_;
    QList<DiffCalculation::LineDiffResult> lineDiffs_;
    QVector<ChangeRegion> regions_;
    Actions actionMode_ = Actions::None;
    int currentRegion_ = -1;

    QLabel *oldTitleLabel_ = nullptr;
    QLabel *newTitleLabel_ = nullptr;
    QLabel *changeLabel_ = nullptr;
    QPushButton *previousButton_ = nullptr;
    QPushButton *nextButton_ = nullptr;

    QTextEdit *numberOldEdit_ = nullptr;
    QTextEdit *oldEdit_ = nullptr;
    QWidget *gutter_ = nullptr;
    QTextEdit *numberNewEdit_ = nullptr;
    QTextEdit *newEdit_ = nullptr;
};
