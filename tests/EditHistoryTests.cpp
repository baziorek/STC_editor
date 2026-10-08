// Tests of EditHistory: the record of edits shown by "Edit history" and by the circles at line numbers.
// The documents are edited the way a user edits them (key events sent to a QPlainTextEdit), so Qt groups typing
// into undo steps exactly as in the editor.
#include <gtest/gtest.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

#include "utils/DiffCalculation.h"
#include "utils/EditHistory.h"

namespace
{
using Kind = EditHistory::LineKind;
using Marker = EditHistory::LineMarker;

class EditHistoryTest : public ::testing::Test
{
protected:
    QPlainTextEdit edit;
    EditHistory history{ edit.document() };

    void key(int keyCode, const QString& text = QString(), Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        QKeyEvent press(QEvent::KeyPress, keyCode, modifiers, text);
        QApplication::sendEvent(&edit, &press);
        QKeyEvent release(QEvent::KeyRelease, keyCode, modifiers, text);
        QApplication::sendEvent(&edit, &release);
    }

    void type(const QString& text)
    {
        for (const QChar ch : text)
        {
            if (ch == QLatin1Char('\n'))
                key(Qt::Key_Return, QStringLiteral("\r"));
            else if (ch == QLatin1Char(' '))
                key(Qt::Key_Space, QStringLiteral(" "));
            else
                key(Qt::Key_A, QString(ch));
        }
    }

    void enter() { key(Qt::Key_Return, QStringLiteral("\r")); }
    void backspace() { key(Qt::Key_Backspace); }

    void undo()
    {
        EditHistory::UndoRedoScope scope(history, EditHistory::UndoRedoScope::Kind::Undo);
        edit.undo();
    }

    void redo()
    {
        EditHistory::UndoRedoScope scope(history, EditHistory::UndoRedoScope::Kind::Redo);
        edit.redo();
    }

    void moveCursorToLine(int line, int column = 0)
    {
        const QTextBlock block = edit.document()->findBlockByNumber(line);
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + std::min(column, block.length() - 1));
        edit.setTextCursor(cursor);
    }

    void expectConsistent()
    {
        QString problem;
        EXPECT_TRUE(history.verifyConsistency(&problem)) << problem.toStdString();
    }
};
} // namespace


TEST_F(EditHistoryTest, StartsEmptyAtStateZero)
{
    EXPECT_EQ(history.stepCount(), 0);
    EXPECT_EQ(history.currentIndex(), 0);
    EXPECT_EQ(history.lineCount(), 1);
    EXPECT_EQ(history.lineMarker(0), Marker::None);
    expectConsistent();
}

TEST_F(EditHistoryTest, TypingAWordIsOneStep)
{
    type(QStringLiteral("hello world"));

    ASSERT_EQ(history.stepCount(), 1);
    EXPECT_EQ(history.currentIndex(), 1);

    const auto info = history.stepInfo(1);
    EXPECT_EQ(info.modifiedLines, 1);
    EXPECT_EQ(info.addedLines, 0);
    EXPECT_EQ(info.removedLines, 0);
    ASSERT_EQ(info.modified.size(), 1);
    EXPECT_EQ(info.modified[0].first, 1);
    EXPECT_EQ(info.modified[0].last, 1);
    EXPECT_EQ(info.charsInserted, 11);
    EXPECT_EQ(info.charsRemoved, 0);
    EXPECT_TRUE(info.isCurrent);
    EXPECT_TRUE(info.applied);

    EXPECT_EQ(history.lineMarker(0), Marker::Applied);
    EXPECT_EQ(history.lineChangeCount(0), 1);
    expectConsistent();
}

TEST_F(EditHistoryTest, EnterAndFurtherTypingAreSeparateSteps)
{
    type(QStringLiteral("abc"));
    enter();
    type(QStringLiteral("def"));

    ASSERT_EQ(history.stepCount(), 3);

    const auto enterStep = history.stepInfo(2);
    EXPECT_EQ(enterStep.addedLines, 1);
    EXPECT_EQ(enterStep.modifiedLines, 0);
    ASSERT_EQ(enterStep.added.size(), 1);
    EXPECT_EQ(enterStep.added[0].first, 2);
    EXPECT_EQ(enterStep.charsInserted, 1); // the line break

    const auto secondTyping = history.stepInfo(3);
    EXPECT_EQ(secondTyping.modifiedLines, 1);
    EXPECT_EQ(secondTyping.modified[0].first, 2);
    EXPECT_EQ(secondTyping.charsInserted, 3);

    // line 2 was added and then typed in: two entries; line 1 was only typed in
    EXPECT_EQ(history.lineChangeCount(0), 1);
    EXPECT_EQ(history.lineChangeCount(1), 2);
    expectConsistent();
}

TEST_F(EditHistoryTest, UndoAndRedoMoveTheIndexAndKeepTheHistory)
{
    type(QStringLiteral("abc"));
    enter();
    type(QStringLiteral("def"));
    ASSERT_EQ(history.currentIndex(), 3);

    undo();
    EXPECT_EQ(history.currentIndex(), 2);
    EXPECT_EQ(history.stepCount(), 3); // the undone step can still be seen (and redone)
    EXPECT_FALSE(history.stepInfo(3).applied);
    EXPECT_TRUE(history.stepInfo(2).isCurrent);
    EXPECT_EQ(edit.toPlainText(), QStringLiteral("abc\n"));
    EXPECT_EQ(history.lineMarker(1), Marker::Applied); // the empty line 2 still has its "added" entry

    undo();
    EXPECT_EQ(history.currentIndex(), 1);
    EXPECT_EQ(history.lineCount(), 1);

    redo();
    EXPECT_EQ(history.currentIndex(), 2);
    redo();
    EXPECT_EQ(history.currentIndex(), 3);
    EXPECT_EQ(edit.toPlainText(), QStringLiteral("abc\ndef"));
    EXPECT_EQ(history.lineChangeCount(1), 2); // the line got its history back
    expectConsistent();
}

TEST_F(EditHistoryTest, UndoWithNothingToUndoChangesNothing)
{
    undo();
    redo();
    EXPECT_EQ(history.currentIndex(), 0);

    type(QStringLiteral("x"));
    redo(); // nothing to redo
    EXPECT_EQ(history.currentIndex(), 1);
    expectConsistent();
}

TEST_F(EditHistoryTest, NewEditAfterUndoDropsWhatCouldHaveBeenRedone)
{
    type(QStringLiteral("abc"));
    enter();
    type(QStringLiteral("def"));
    undo();
    undo();
    ASSERT_EQ(history.currentIndex(), 1);
    ASSERT_EQ(history.stepCount(), 3);

    key(Qt::Key_End);
    enter(); // a new edit: steps 2 and 3 are gone for good

    EXPECT_EQ(history.stepCount(), 2);
    EXPECT_EQ(history.currentIndex(), 2);
    EXPECT_EQ(history.lineChangeCount(1), 1); // only the new "added" entry, not the dropped ones
    expectConsistent();

    redo(); // nothing to redo any more
    EXPECT_EQ(history.currentIndex(), 2);
}

TEST_F(EditHistoryTest, UndoneChangesKeepTheirCircleUntilNewEditsAreMade)
{
    type(QStringLiteral("one"));
    enter();
    type(QStringLiteral("two"));
    undo(); // removes "two" from line 2

    // line 2 still exists (empty) and still has history: step 2 added it, step 3 (undone) typed in it
    ASSERT_EQ(history.lineCount(), 2);
    const auto lineHistory = history.lineHistory(1);
    ASSERT_EQ(lineHistory.entries.size(), 2);
    EXPECT_TRUE(lineHistory.entries[0].applied);
    EXPECT_FALSE(lineHistory.entries[1].applied);
    EXPECT_EQ(lineHistory.currentIndex, 2);
    EXPECT_EQ(lineHistory.stepCount, 3);

    undo(); // the line itself is gone now: it is part of what can be redone
    EXPECT_EQ(history.lineCount(), 1);
    EXPECT_EQ(history.lineMarker(0), Marker::Applied);

    redo();
    redo();
    EXPECT_EQ(history.lineMarker(1), Marker::Applied);
    EXPECT_EQ(history.lineHistory(1).entries.size(), 2);
}

TEST_F(EditHistoryTest, ALineWhoseEveryChangeWasUndoneIsMarkedAsUndoneOnly)
{
    type(QStringLiteral("one"));
    enter();
    type(QStringLiteral("two"));
    enter();
    type(QStringLiteral("three"));
    moveCursorToLine(0, 1);
    type(QStringLiteral("X")); // step 6 changes only line 1 ("oXne")
    ASSERT_EQ(history.currentIndex(), 6);

    // go back to before line 1 was ever edited, but stay inside the history: line 1 gets "oXne" removed,
    // so it is still changed (the typing of "one" is applied)
    undo();
    EXPECT_EQ(history.lineMarker(0), Marker::Applied);
    EXPECT_EQ(history.lineHistory(0).entries.size(), 2);
    EXPECT_TRUE(history.lineHistory(0).entries[0].applied);
    EXPECT_FALSE(history.lineHistory(0).entries[1].applied);
}

TEST_F(EditHistoryTest, SavesAreRemembered)
{
    type(QStringLiteral("abc"));
    history.noteSaved();
    enter();
    type(QStringLiteral("d"));
    history.noteSaved();

    const auto marks = history.saveMarks();
    ASSERT_EQ(marks.size(), 2);
    EXPECT_EQ(marks[0].state, 1);
    EXPECT_EQ(marks[1].state, 3);
    EXPECT_EQ(history.stepInfo(1).saves.size(), 1);
    EXPECT_EQ(history.stepInfo(2).saves.size(), 0);
    EXPECT_EQ(history.stepInfo(3).saves.size(), 1);

    // the change on line 1 was first written by the first save, the one on line 2 by the second
    EXPECT_TRUE(history.lineHistory(0).entries[0].savedAt.isValid());
    EXPECT_EQ(history.lineHistory(0).entries[0].savedAt, marks[0].time);
    EXPECT_EQ(history.lineHistory(1).entries.last().savedAt, marks[1].time);
}

TEST_F(EditHistoryTest, ASavedStateWhichIsDroppedIsMarkedSo)
{
    type(QStringLiteral("a"));
    enter();
    history.noteSaved(); // state 2
    undo();              // back to state 1
    type(QStringLiteral("b")); // a different future: state 2 of the file on disk does not exist any more

    const auto marks = history.saveMarks();
    ASSERT_EQ(marks.size(), 1);
    EXPECT_TRUE(marks[0].stateDiscarded);
    EXPECT_EQ(history.stepInfo(2).saves.size(), 0);
}

TEST_F(EditHistoryTest, LinesKeepTheirHistoryWhenLinesAreInsertedAboveThem)
{
    type(QStringLiteral("one\ntwo\nthree"));
    const int twoId = history.lineId(1);
    const int twoChanges = history.lineChangeCount(1);
    ASSERT_GE(twoId, 0);

    moveCursorToLine(0);
    enter(); // an empty line above everything

    EXPECT_EQ(history.lineOfId(twoId), 2);
    EXPECT_EQ(history.lineChangeCount(2), twoChanges);
    EXPECT_EQ(history.lineMarker(0), Marker::Applied); // the new empty line was added: it has an entry
    EXPECT_EQ(history.lineHistory(0).entries.size(), 1);
    EXPECT_EQ(history.lineHistory(0).entries[0].kind, Kind::Added);
    expectConsistent();
}

TEST_F(EditHistoryTest, SplittingALineKeepsTheHistoryOnTheFirstPart)
{
    type(QStringLiteral("hello world"));
    const int id = history.lineId(0);

    moveCursorToLine(0, 5);
    enter();

    EXPECT_EQ(history.lineId(0), id);          // "hello" is still the same line
    EXPECT_NE(history.lineId(1), id);          // " world" is a new one
    EXPECT_EQ(history.lineHistory(1).entries.size(), 1);
    EXPECT_EQ(history.lineHistory(1).entries[0].kind, Kind::Added);
    EXPECT_EQ(history.lineHistory(0).entries.size(), 2); // typed, then split
    expectConsistent();
}

TEST_F(EditHistoryTest, JoiningLinesRemovesTheSecondOne)
{
    type(QStringLiteral("ab\ncd"));
    moveCursorToLine(1);
    const int firstId = history.lineId(0);
    backspace(); // joins the lines

    ASSERT_EQ(history.lineCount(), 1);
    EXPECT_EQ(history.lineId(0), firstId);
    const auto info = history.stepInfo(history.currentIndex());
    EXPECT_EQ(info.removedLines, 1);
    EXPECT_EQ(info.modifiedLines, 1);
    EXPECT_EQ(info.charsRemoved, 1); // just the line break
    EXPECT_EQ(info.charsInserted, 0);
    expectConsistent();
}

TEST_F(EditHistoryTest, UndoBringsBackARemovedLineTogetherWithItsHistory)
{
    type(QStringLiteral("a\nb\nc"));
    const int idOfB = history.lineId(1);
    const int changesOfB = history.lineChangeCount(1);

    {
        QTextCursor cursor(edit.document()->findBlockByNumber(1));
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
    }
    ASSERT_EQ(edit.toPlainText(), QStringLiteral("a\nc"));
    EXPECT_EQ(history.lineOfId(idOfB), -1);
    const auto removal = history.stepInfo(history.currentIndex());
    EXPECT_EQ(removal.removedLines, 1);
    ASSERT_EQ(removal.removed.size(), 1);
    EXPECT_EQ(removal.removed[0].first, 2); // line numbers before the step

    undo();
    EXPECT_EQ(edit.toPlainText(), QStringLiteral("a\nb\nc"));
    EXPECT_EQ(history.lineOfId(idOfB), 1);
    const auto lineHistory = history.lineHistoryById(idOfB);
    ASSERT_EQ(lineHistory.entries.size(), changesOfB + 1);
    EXPECT_EQ(lineHistory.entries.last().kind, Kind::Removed);
    EXPECT_FALSE(lineHistory.entries.last().applied);

    redo();
    EXPECT_EQ(history.lineOfId(idOfB), -1);
    expectConsistent();
}

TEST_F(EditHistoryTest, ChangeOfFormattingOnlyIsAStepWithoutChangedLines)
{
    type(QStringLiteral("abcdef"));
    QTextCursor cursor(edit.document());
    cursor.setPosition(0);
    cursor.setPosition(3, QTextCursor::KeepAnchor);
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);
    cursor.mergeCharFormat(format);

    ASSERT_EQ(history.stepCount(), 2);
    EXPECT_EQ(history.stepInfo(2).changedLines(), 0);
    EXPECT_EQ(history.stepInfo(2).charsInserted, 0);
    EXPECT_EQ(history.stepInfo(2).charsRemoved, 0);

    undo();
    EXPECT_EQ(history.currentIndex(), 1);
    expectConsistent();
}

TEST_F(EditHistoryTest, ReplacingTheWholeContentStartsTheHistoryOver)
{
    type(QStringLiteral("abc"));
    history.noteSaved();
    ASSERT_EQ(history.stepCount(), 1);

    edit.setPlainText(QStringLiteral("x\ny\nz")); // opening another file

    EXPECT_EQ(history.stepCount(), 0);
    EXPECT_EQ(history.currentIndex(), 0);
    EXPECT_EQ(history.lineCount(), 3);
    EXPECT_TRUE(history.saveMarks().isEmpty());
    EXPECT_EQ(history.lineMarker(0), Marker::None);
    expectConsistent();

    type(QStringLiteral("q"));
    EXPECT_EQ(history.stepCount(), 1);

    edit.clear();
    EXPECT_EQ(history.stepCount(), 0);
    expectConsistent();
}

TEST_F(EditHistoryTest, ClearingTheDocumentDirectlyStartsTheHistoryOverToo)
{
    type(QStringLiteral("abc"));
    edit.document()->clear(); // undo stays enabled here; Qt only drops the stacks
    EXPECT_EQ(history.stepCount(), 0);
    expectConsistent();
}

TEST_F(EditHistoryTest, KeepsNonBreakingSpacesAndLineSeparatorsInsideTheLine)
{
    edit.setPlainText(QStringLiteral("a\u00A0b\u2028c\nd"));
    ASSERT_EQ(history.lineCount(), 2);
    expectConsistent();

    moveCursorToLine(1, 1);
    type(QStringLiteral("x"));
    EXPECT_EQ(history.stepInfo(1).charsInserted, 1);
    expectConsistent();
}

TEST_F(EditHistoryTest, StepDiffShowsTheLinesWithRealNumbers)
{
    type(QStringLiteral("alpha"));
    enter();
    type(QStringLiteral("beta"));

    const auto diff = history.stepDiff(3);
    ASSERT_EQ(diff.size(), 1);
    EXPECT_EQ(diff[0].newLineIndex, 1);
    EXPECT_EQ(diff[0].oldLineIndex, 1);

    const auto diffOfEnter = history.stepDiff(2); // line "alpha" is kept as context, the new line is added
    ASSERT_EQ(diffOfEnter.size(), 2);
    EXPECT_EQ(diffOfEnter[0].newLineIndex, 0);
    EXPECT_EQ(diffOfEnter[1].newLineIndex, 1);
    EXPECT_EQ(diffOfEnter[1].oldLineIndex, -1);
}

TEST_F(EditHistoryTest, StepLineInEditorFollowsTheLine)
{
    type(QStringLiteral("one\ntwo\nthree"));
    ASSERT_EQ(history.stepCount(), 5);
    EXPECT_EQ(history.stepLineInEditor(5), 2);

    moveCursorToLine(0);
    enter(); // everything moves one line down
    EXPECT_EQ(history.stepLineInEditor(5), 3);
}

TEST_F(EditHistoryTest, TextBudgetDropsTheTextOfTheOldestStepsButKeepsTheNumbers)
{
    history.setTextBudget(3000);
    for (int i = 0; i < 80; ++i)
    {
        type(QStringLiteral("some words of a line number"));
        enter();
    }
    ASSERT_EQ(history.stepCount(), 160);
    EXPECT_LE(history.retainedCharacters(), 3000);

    const auto first = history.stepInfo(1);
    EXPECT_TRUE(first.detailsDiscarded);
    EXPECT_EQ(first.charsInserted, 27); // the numbers survived
    EXPECT_TRUE(history.stepDiff(1).isEmpty());
    EXPECT_FALSE(history.stepInfo(160).detailsDiscarded);

    // the history keeps working across the steps which lost their text
    const int total = history.stepCount();
    for (int i = 0; i < total; ++i)
        undo();
    EXPECT_EQ(history.currentIndex(), 0);
    expectConsistent();
    for (int i = 0; i < total; ++i)
        redo();
    EXPECT_EQ(history.currentIndex(), total);
    EXPECT_EQ(history.lineChangeCount(0), 1);
    EXPECT_TRUE(history.lineHistory(0).entries[0].detailsDiscarded);
    expectConsistent();
}

TEST_F(EditHistoryTest, ABigDocumentIsNotCopiedAtEveryKeystroke)
{
    QString text;
    for (int i = 0; i < 100000; ++i)
        text += QStringLiteral("line number %1 of a rather long document\n").arg(i);
    edit.setPlainText(text);
    ASSERT_EQ(history.lineCount(), 100001);

    moveCursorToLine(50000, 5);
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 300; ++i)
    {
        type(QStringLiteral("typing a few words here"));
        enter();
    }
    const qint64 elapsed = timer.elapsed();
    // 7000 key presses and 300 line breaks in a document of 100 000 lines
    EXPECT_LT(elapsed, 8000) << "took " << elapsed << " ms";
    EXPECT_EQ(history.stepCount(), 600); // 300 x (typing + Enter)
    expectConsistent();
}

// A user's editing session made of random operations. After every operation everything we keep must agree with the
// document, and our idea of "the current state" must be exactly the number of Ctrl+Z which reaches the start.
// A few seeds always run; EDIT_HISTORY_FUZZ_SEEDS=300 runs many more (for a deeper check after changing EditHistory).
class EditHistoryRandomTest : public EditHistoryTest
{
protected:
    void runSession(quint32 seed);
};

TEST_F(EditHistoryRandomTest, RandomEditingAgreesWithTheUndoStackOfQt)
{
    const int seeds = qEnvironmentVariableIntValue("EDIT_HISTORY_FUZZ_SEEDS") > 0 ? qEnvironmentVariableIntValue("EDIT_HISTORY_FUZZ_SEEDS") : 6;
    for (int seed = 1; seed <= seeds; ++seed)
    {
        SCOPED_TRACE(testing::Message() << "seed " << seed);
        edit.clear();
        runSession(static_cast<quint32>(seed) * 7919u);
        if (HasFatalFailure())
            return;
    }
}

void EditHistoryRandomTest::runSession(quint32 seed)
{
    QRandomGenerator random(seed);
    auto pick = [&random](int bound) { return static_cast<int>(random.bounded(bound)); };

    const QStringList words = { "alpha", "beta ", "gamma", "delta ", "x", " ", "zażółć ", "gęślą" };
    edit.setPlainText(QStringLiteral("first line\nsecond line\n\nfourth\nfifth line here\nsixth"));
    history.reset();
    expectConsistent();
    const int restartsAtStart = history.restartCount();

    for (int iteration = 0; iteration < 700; ++iteration)
    {
        const int lineCount = edit.document()->blockCount();
        const int operation = pick(16);
        switch (operation)
        {
        case 0:
        case 1:
        case 2:
            moveCursorToLine(pick(lineCount), pick(6));
            type(words[pick(words.size())]);
            break;
        case 3:
            moveCursorToLine(pick(lineCount), pick(6));
            enter();
            break;
        case 4:
            moveCursorToLine(pick(lineCount), pick(4));
            backspace();
            if (pick(2))
                backspace();
            break;
        case 5:
            moveCursorToLine(pick(lineCount), 0);
            key(Qt::Key_Delete);
            break;
        case 6:
            moveCursorToLine(pick(lineCount), pick(5));
            edit.insertPlainText(QStringLiteral("pasted\nlines\nhere"));
            break;
        case 7:
        {
            // select a stretch of text (maybe over lines) and type over it
            QTextCursor cursor(edit.document());
            const int max = edit.document()->characterCount() - 1;
            const int start = pick(max + 1);
            cursor.setPosition(start);
            cursor.setPosition(std::min(max, start + pick(25)), QTextCursor::KeepAnchor);
            edit.setTextCursor(cursor);
            type(QStringLiteral("Q"));
            break;
        }
        case 8:
        {
            // replace in many places at once, as one step
            QTextCursor all(edit.document());
            all.beginEditBlock();
            QTextCursor found = edit.document()->find(QStringLiteral("e"), 0);
            int count = 0;
            while (!found.isNull() && count++ < 6)
            {
                found.insertText(QStringLiteral("EE"));
                found = edit.document()->find(QStringLiteral("e"), found);
            }
            all.endEditBlock();
            break;
        }
        case 9:
        {
            // remove whole lines
            if (lineCount > 3)
            {
                QTextCursor cursor(edit.document()->findBlockByNumber(pick(lineCount - 1)));
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, 1 + pick(2));
                cursor.removeSelectedText();
                cursor.endEditBlock();
            }
            break;
        }
        case 10:
        case 11:
            for (int i = 1 + pick(3); i > 0; --i)
                undo();
            break;
        case 12:
            for (int i = 1 + pick(2); i > 0; --i)
                redo();
            break;
        case 13:
            edit.document()->setModified(false);
            history.noteSaved();
            break;
        case 14:
        {
            QTextCursor cursor(edit.document());
            cursor.setPosition(0);
            cursor.setPosition(std::min(3, edit.document()->characterCount() - 1), QTextCursor::KeepAnchor);
            QTextCharFormat format;
            format.setFontItalic(pick(2));
            cursor.mergeCharFormat(format);
            break;
        }
        default:
            // lines inserted at the very top and at the very bottom
            moveCursorToLine(pick(2) ? 0 : lineCount - 1, 0);
            if (pick(2))
                key(Qt::Key_End, QString(), Qt::ControlModifier);
            type(QStringLiteral("new\ntext"));
            break;
        }

        QString problem;
        ASSERT_EQ(history.restartCount(), restartsAtStart)
            << "the history started over by itself (" << history.lastRestartReason() << ") in iteration " << iteration << ", operation " << operation;
        ASSERT_TRUE(history.verifyConsistency(&problem)) << "iteration " << iteration << " operation " << operation << ": " << problem.toStdString();

        if (iteration % 25 == 0)
        {
            // The whole point: our idea of "the current state" is exactly the number of Ctrl+Z needed to reach the start
            const QString textBefore = edit.toPlainText();
            const int currentBefore = history.currentIndex();
            const int totalBefore = history.stepCount();

            int undone = 0;
            while (edit.document()->isUndoAvailable())
            {
                undo();
                ++undone;
            }
            ASSERT_EQ(undone, currentBefore) << "iteration " << iteration;
            ASSERT_TRUE(history.verifyConsistency(&problem)) << "after undoing everything, iteration " << iteration << ": " << problem.toStdString();

            int redone = 0;
            while (edit.document()->isRedoAvailable())
            {
                redo();
                ++redone;
            }
            ASSERT_EQ(redone, totalBefore) << "iteration " << iteration;
            ASSERT_TRUE(history.verifyConsistency(&problem)) << "after redoing everything, iteration " << iteration << ": " << problem.toStdString();

            for (int i = totalBefore; i > currentBefore; --i)
                undo();
            ASSERT_EQ(history.currentIndex(), currentBefore);
            ASSERT_EQ(edit.toPlainText(), textBefore) << "iteration " << iteration;
            ASSERT_TRUE(history.verifyConsistency(&problem)) << "iteration " << iteration << ": " << problem.toStdString();
        }
    }

    EXPECT_GT(history.stepCount(), 20);
    if (qEnvironmentVariableIsSet("EDIT_HISTORY_FUZZ_VERBOSE"))
        qInfo() << "seed" << seed << "ended with" << edit.document()->blockCount() << "lines," << edit.document()->characterCount() << "chars," << history.stepCount() << "steps";
}


TEST(DiffCalculationHelpers, CountsInsertedAndRemovedCharacters)
{
    using DiffCalculation::countCharChanges;

    auto counts = countCharChanges(QStringLiteral("hello world"), QStringLiteral("hello brave world"));
    EXPECT_EQ(counts.inserted, 6);
    EXPECT_EQ(counts.removed, 0);
    EXPECT_FALSE(counts.approximate);

    counts = countCharChanges(QStringLiteral("ab\ncd"), QStringLiteral("abcd"));
    EXPECT_EQ(counts.removed, 1);
    EXPECT_EQ(counts.inserted, 0);

    counts = countCharChanges(QStringLiteral("same"), QStringLiteral("same"));
    EXPECT_EQ(counts.inserted + counts.removed, 0);

    counts = countCharChanges(QStringLiteral("cat and dog"), QStringLiteral("cow and dig"));
    EXPECT_EQ(counts.removed, 3); // a, t, o
    EXPECT_EQ(counts.inserted, 3); // o, w, i

    // characters are code points, not UTF-16 units
    counts = countCharChanges(QString::fromUtf8("a\xF0\x9F\x98\x80" "b"), QString::fromUtf8("a\xF0\x9F\x98\x81" "b"));
    EXPECT_EQ(counts.removed, 1);
    EXPECT_EQ(counts.inserted, 1);
}

TEST(DiffCalculationHelpers, InlineDiffKeepsTheOrderOfTheText)
{
    using DiffCalculation::FragmentType;
    const auto fragments = DiffCalculation::computeInlineDiff(QStringLiteral("abc"), QStringLiteral("abXc"));
    ASSERT_EQ(fragments.size(), 3);
    EXPECT_EQ(fragments[0].type, FragmentType::Equal);
    EXPECT_EQ(fragments[0].text, QStringLiteral("ab"));
    EXPECT_EQ(fragments[1].type, FragmentType::Insert);
    EXPECT_EQ(fragments[1].text, QStringLiteral("X"));
    EXPECT_EQ(fragments[2].type, FragmentType::Equal);
    EXPECT_EQ(fragments[2].text, QStringLiteral("c"));

    const auto added = DiffCalculation::computeInlineDiff(QString(), QStringLiteral("new"));
    ASSERT_EQ(added.size(), 1);
    EXPECT_EQ(added[0].type, FragmentType::Insert);

    const auto removed = DiffCalculation::computeInlineDiff(QStringLiteral("gone"), QString());
    ASSERT_EQ(removed.size(), 1);
    EXPECT_EQ(removed[0].type, FragmentType::Delete);
}


int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
