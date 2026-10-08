#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>
#include <vector>

#include "utils/DiffCalculation.h"

class QTextDocument;

/// Remembers how a document was edited during this session, so that the editor can show it - without keeping
/// a copy of the document for every edit.
///
/// It only WATCHES the QTextDocument. Qt's undo stack stays exactly as it is, including the way Qt groups typing into
/// one undo step (a run of typed characters, not a single letter): a "step" here is one step of that stack, so one
/// Ctrl+Z moves the current state back by one step and Ctrl+Shift+Z forward by one step.
///
/// What is stored per step: only the lines the step touched (before and after it) and a few numbers, never the whole
/// document. Typing a word costs one line, not one document. The only copy of the whole text is `lines`, one for the
/// entire history; it is what lets us see what a change removed (Qt reports only WHERE and HOW MUCH). A budget of
/// characters (see setTextBudget()) keeps the stored lines bounded: when it is exceeded, the oldest steps lose their
/// text but keep their numbers.
///
/// Every line also has an identity (an id), which survives edits of the line and lines inserted or removed above it,
/// and which comes back on undo. The identity is what the per-line history hangs on.
///
/// Undo and redo are the only things that cannot be told from an ordinary edit by looking at the document, so they
/// have to go through UndoRedoScope (or undo()/redo() of this class). Anything else that changes the document is
/// recorded as a new step; replacing the whole content (setPlainText(), clear()) starts the history from scratch.
class EditHistory : public QObject
{
    Q_OBJECT

public:
    /// A range of line numbers as the user sees them: counted from 1, both ends included
    struct LineRange
    {
        int first = 0;
        int last = 0;
    };

    /// Everything the table of edits says about one step
    struct StepInfo
    {
        int number = 0;                ///< 0 = the state the document started in, then 1... in the order the steps were made
        QDateTime time;                ///< when the step began (for number 0: when the content was loaded)
        QDateTime lastEditTime;        ///< the last edit merged into the step (typing a few words is one step)
        bool applied = true;           ///< false: the step is undone and waits for redo
        bool isCurrent = false;        ///< the document is in the state this step ended with
        int modifiedLines = 0;
        int addedLines = 0;
        int removedLines = 0;
        QList<LineRange> modified;     ///< line numbers after the step
        QList<LineRange> added;        ///< line numbers after the step
        QList<LineRange> removed;      ///< line numbers before the step
        int charsInserted = 0;
        int charsRemoved = 0;
        bool charsApproximate = false;
        QList<QDateTime> saves;        ///< the file was written while the document was in exactly this state
        bool detailsDiscarded = false; ///< the text of the step was dropped to save memory (the numbers are still here)

        int changedLines() const { return modifiedLines + addedLines + removedLines; }
    };

    struct SaveMark
    {
        int state = 0;                 ///< the state of the document (number of applied steps) which was written to disk
        QDateTime time;
        bool stateDiscarded = false;   ///< that state is gone: new edits were made after an undo
    };

    enum class LineKind
    {
        Modified,
        Added,
        Removed
    };

    struct LineEntry
    {
        int step = 0;
        QDateTime time;
        bool applied = true;           ///< false: undone, waits for redo
        bool isCurrent = false;        ///< the document is in the state this step ended with
        LineKind kind = LineKind::Modified;
        QString oldText;
        QString newText;
        QDateTime savedAt;             ///< when the file was FIRST written with this change in it (invalid: never yet)
        bool detailsDiscarded = false;
    };

    struct LineHistory
    {
        int id = -1;
        int line = -1;                 ///< zero-based number of the line in the document now; -1 when it is not there (undone)
        int currentIndex = 0;          ///< how many steps are applied
        int stepCount = 0;
        QList<LineEntry> entries;      ///< oldest first
    };

    enum class LineMarker
    {
        None,        ///< nothing happened to the line in this session
        Applied,     ///< the line was changed (and the change is still in the document)
        UndoneOnly   ///< the line was changed, but every change has been undone
    };

    /// Wraps a call that undoes or redoes (e.g. QPlainTextEdit::undo()), so the history knows what the resulting
    /// change of the document is
    class UndoRedoScope
    {
    public:
        enum class Kind
        {
            Undo,
            Redo
        };

        UndoRedoScope(EditHistory& history, Kind kind);
        ~UndoRedoScope();
        UndoRedoScope(const UndoRedoScope&) = delete;
        UndoRedoScope& operator=(const UndoRedoScope&) = delete;

    private:
        EditHistory& history;
        bool outermost;
    };

    explicit EditHistory(QTextDocument* document, QObject* parent = nullptr);
    ~EditHistory() override;

    /// Same as QTextDocument::undo() / redo(), recorded. (A widget calls its own undo() inside an UndoRedoScope.)
    void undo();
    void redo();

    /// Forgets everything and starts again from what the document holds now
    void reset();

    /// The file was written to disk while the document was in the current state
    void noteSaved();

    int stepCount() const { return static_cast<int>(steps.size()); }
    int currentIndex() const { return current; }
    QDateTime baselineTime() const { return baseline; }
    /// How many of the oldest steps lost their text because of the budget (their numbers are still there)
    int discardedStepCount() const;
    QList<SaveMark> saveMarks() const { return saves; }

    StepInfo stepInfo(int number) const;
    /// The lines the step changed, ready for DiffViewerWidget (numbers are real line numbers). Empty if the text is gone.
    QList<DiffCalculation::LineDiffResult> stepDiff(int number) const;
    /// Zero-based line of the document the step is about (where it is now, if the line still exists); -1 if none
    int stepLineInEditor(int number) const;

    int lineCount() const { return static_cast<int>(lines.size()); }
    LineMarker lineMarker(int line) const;
    int lineChangeCount(int line) const;
    int lineId(int line) const;
    int lineOfId(int id) const;
    LineHistory lineHistory(int line) const { return lineHistoryById(lineId(line)); }
    LineHistory lineHistoryById(int id) const;

    /// How much text (in QChars) the steps may keep. When it is exceeded the oldest steps lose their text.
    /// It is counted generously: the new text of a line in one step is the old text of the next step which changes
    /// that line, and it is stored once (Qt shares it) but counted twice.
    void setTextBudget(qint64 characters);
    qint64 retainedCharacters() const { return retained; }

    /// For tests: is everything we keep in agreement with the document? (O(size of the document))
    bool verifyConsistency(QString* problem = nullptr) const;
    /// For tests: how many times the history started over by itself (the content was replaced, or we lost track)
    int restartCount() const { return restarts; }
    const char* lastRestartReason() const { return restartReason; }

signals:
    /// Steps, the current state, saves or line markers changed
    void changed();

private slots:
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void onUndoCommandAdded();

private:
    enum class Operation
    {
        Edit,
        Undo,
        Redo
    };

    struct Step
    {
        QDateTime time;
        QDateTime lastTime;
        int first = 0;                 ///< the first line the step touched; it is the same number before and after the step
        QList<QString> oldLines;       ///< the lines the step replaced, as they were ...
        QList<QString> newLines;       ///< ... and as they are after the step
        QList<int> oldIds;             ///< identities of those lines, parallel to oldLines / newLines (kept when the text is dropped)
        QList<int> newIds;
        QList<int> touchedIds;         ///< the lines which list this step in `touches`
        bool detailsDiscarded = false;
        mutable std::optional<StepInfo> summary;

        qint64 characters() const;
    };

    /// What a single change of the document did to the lines
    struct Region
    {
        int first = 0;
        QList<QString> oldLines;
        QList<QString> newLines;
        QList<int> oldIds;
        QList<int> newIds;
        QList<int> touched;            ///< ids of the lines modified, added or removed
    };

    void resync(const char* reason);
    bool computeRegion(int position, int charsRemoved, int charsAdded, bool allocateIds, Region& region);
    void applyRegion(const Region& region);
    void beginStep(const Region& region);
    void extendStep(Step& step, const Region& region);
    void recordTouches(Step& step, int number, const QList<int>& ids);
    void dropRedoBranch();
    void finishOperation();
    bool restoreIds(const Step& step, bool oldSide);
    void assignMissingIds();
    void enforceBudget();
    void discardDetails(Step& step);
    const StepInfo& summaryOf(const Step& step) const;
    StepInfo computeSummary(const Step& step) const;

    static constexpr qint64 defaultTextBudget = 16'000'000; // QChars: about 32 MB

    QTextDocument* document;
    QList<QString> lines;              ///< the text of the document line by line, as it was before the change being handled
    QList<int> ids;                    ///< identity of every line, parallel to `lines`
    int nextId = 0;
    std::vector<Step> steps;           ///< steps[0] is step number 1; those after `current` can be redone
    int current = 0;                   ///< how many steps are applied
    bool newStepPending = false;       ///< Qt has just started a new undo step; the next change belongs to it
    Operation operation = Operation::Edit;
    int operationEvents = 0;
    bool placeholderIds = false;       ///< some lines have no identity yet (-1): undo/redo is being applied
    QHash<int, QList<int>> touches;    ///< line id -> numbers of the steps which changed it (ascending)
    QList<SaveMark> saves;
    QDateTime baseline;
    qint64 budget = defaultTextBudget;
    qint64 retained = 0;
    int restarts = 0;
    const char* restartReason = "";
};
