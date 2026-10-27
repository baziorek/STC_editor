#include <gtest/gtest.h>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTest>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QThread>
#include <memory>

#include "widgets/ConsoleInputEdit.h"
#include "widgets/ConsoleOutputEdit.h"
#include "widgets/ConsoleWidget.h"

// These tests run real commands through /bin/sh.

namespace
{
/// Waits (processing events) until the console has no running command; false after the timeout.
bool waitUntilIdle(const ConsoleWidget& console, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (console.isRunning() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return !console.isRunning();
}

bool waitUntilRunning(const ConsoleWidget& console, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!console.isRunning() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return console.isRunning();
}

/// Runs the command and waits for its end.
void run(ConsoleWidget& console, const QString& command)
{
    console.runCommand(command);
    ASSERT_TRUE(waitUntilIdle(console)) << qPrintable(command);
}

/// The output without the first line, which is the echo of the typed command (it contains the text of the command itself)
QString resultOf(const ConsoleWidget& console)
{
    const QString text = console.outputText();
    return text.mid(text.indexOf(QLatin1Char('\n')) + 1);
}

ConsoleOutputEdit* outputOf(ConsoleWidget& console)
{
    return console.findChild<ConsoleOutputEdit*>();
}

/// The foreground color of the first character of `text` in the output, invalid when it has no own color.
QColor colorOf(ConsoleWidget& console, const QString& text)
{
    QTextDocument* document = outputOf(console)->document();
    const QTextCursor afterEcho(document->firstBlock().next());
    const QTextCursor found = document->find(text, afterEcho);
    if (found.isNull())
        return {};
    QTextCursor cursor(document);
    cursor.setPosition(found.selectionStart() + 1); // the format is that of the character before the cursor
    const QTextCharFormat format = cursor.charFormat();
    return format.hasProperty(QTextFormat::ForegroundBrush) ? format.foreground().color() : QColor();
}

ConsoleInputEdit* inputOf(ConsoleWidget& console)
{
    return console.findChild<ConsoleInputEdit*>();
}

QString canonical(const QString& path)
{
    return QFileInfo(path).canonicalFilePath();
}
} // namespace

class ConsoleWidgetTest : public ::testing::Test
{
protected:
    ConsoleWidgetTest()
    {
        console.resize(600, 300);
        console.setWorkingDirectory(directory.path());
    }

    QTemporaryDir directory;
    ConsoleWidget console;
};

TEST_F(ConsoleWidgetTest, StartsInTheGivenDirectory)
{
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.path()));
}

TEST_F(ConsoleWidgetTest, ShowsTheOutputOfACommand)
{
    run(console, "echo hello-console");
    EXPECT_TRUE(console.outputText().contains("hello-console"));
    EXPECT_TRUE(console.outputText().contains("echo hello-console")); // the typed command is echoed
}

TEST_F(ConsoleWidgetTest, RunsInTheWorkingDirectory)
{
    run(console, "pwd");
    EXPECT_TRUE(console.outputText().contains(canonical(directory.path())));
}

TEST_F(ConsoleWidgetTest, ShowsTheErrorOutputToo)
{
    run(console, "echo to-stderr 1>&2");
    EXPECT_TRUE(console.outputText().contains("to-stderr"));
}

TEST_F(ConsoleWidgetTest, ReportsAFailedCommand)
{
    QSignalSpy finished(&console, &ConsoleWidget::commandFinished);
    run(console, "exit 3");
    ASSERT_EQ(finished.count(), 1);
    EXPECT_EQ(finished.at(0).at(0).toInt(), 3);
    EXPECT_TRUE(console.outputText().contains("[exit code 3]"));
}

TEST_F(ConsoleWidgetTest, ASuccessfulCommandPrintsNoStatus)
{
    run(console, "true");
    EXPECT_FALSE(console.outputText().contains("[exit code"));
}

TEST_F(ConsoleWidgetTest, UnknownProgramIsReportedByTheShell)
{
    run(console, "definitely-not-a-program-xyz");
    EXPECT_TRUE(console.outputText().contains("[exit code 127]"));
}

TEST_F(ConsoleWidgetTest, ColorsOfTheOutputAreShownAndNotPutIntoTheText)
{
    run(console, R"(printf '\033[31mred-text\033[0m plain')");
    EXPECT_TRUE(resultOf(console).contains("red-text plain"));
    EXPECT_FALSE(resultOf(console).contains(QChar(0x1b)));
    EXPECT_FALSE(resultOf(console).contains("[31m"));
    EXPECT_TRUE(colorOf(console, "red-text").isValid());
    EXPECT_EQ(colorOf(console, "plain"), QColor()); // the reset ended the color
}

TEST_F(ConsoleWidgetTest, CarriageReturnRewritesTheLine)
{
    run(console, R"(printf 'progress 10%%\rprogress 90%%\ndone\n')");
    const QString text = resultOf(console);
    EXPECT_FALSE(text.contains("progress 10%"));
    EXPECT_TRUE(text.contains("progress 90%"));
    EXPECT_FALSE(text.contains(QLatin1Char('\r')));
}

TEST_F(ConsoleWidgetTest, WindowsLineEndingsAreOrdinaryLineBreaks)
{
    run(console, R"(printf 'one\r\ntwo\r\n')");
    const QString text = console.outputText();
    EXPECT_TRUE(text.contains("one\ntwo"));
    EXPECT_FALSE(text.contains(QLatin1Char('\r')));
}

TEST_F(ConsoleWidgetTest, Utf8IsDecoded)
{
    run(console, "echo zażółć gęślą jaźń");
    EXPECT_TRUE(console.outputText().contains("zażółć gęślą jaźń"));
}

TEST_F(ConsoleWidgetTest, PipesAndRedirectionsWork)
{
    run(console, "echo abc | tr a-z A-Z > upper.txt && cat upper.txt");
    EXPECT_TRUE(console.outputText().contains("ABC"));
    EXPECT_TRUE(QFile::exists(directory.filePath("upper.txt")));
}

// ------------------------------------------------------------------ cd

TEST_F(ConsoleWidgetTest, CdIsRememberedBetweenCommands)
{
    ASSERT_TRUE(QDir(directory.path()).mkdir("sub"));
    QSignalSpy changed(&console, &ConsoleWidget::workingDirectoryChanged);

    run(console, "cd sub");
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.filePath("sub")));
    EXPECT_EQ(changed.count(), 1);

    run(console, "pwd");
    EXPECT_TRUE(console.outputText().contains(canonical(directory.filePath("sub"))));

    run(console, "cd ..");
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.path()));

    run(console, "cd -");
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetTest, CdToAMissingDirectoryIsAnError)
{
    const QString before = console.workingDirectory();
    run(console, "cd no-such-dir");
    EXPECT_EQ(console.workingDirectory(), before);
    EXPECT_TRUE(console.outputText().contains("no such directory"));
}

TEST_F(ConsoleWidgetTest, CdInAChainIsOnlyForThatLine)
{
    ASSERT_TRUE(QDir(directory.path()).mkdir("sub"));
    run(console, "cd sub && pwd");
    EXPECT_TRUE(console.outputText().contains(canonical(directory.filePath("sub"))));
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.path()));
}

TEST_F(ConsoleWidgetTest, ClearEmptiesTheOutput)
{
    run(console, "echo something");
    ASSERT_FALSE(console.outputText().isEmpty());
    run(console, "clear");
    EXPECT_TRUE(console.outputText().isEmpty());
}

// ------------------------------------------------------------------ the file of the editor

TEST_F(ConsoleWidgetTest, CommandsSeeTheFileOfTheEditor)
{
    const QString file = directory.filePath("article.txt");
    console.setCurrentFileProvider([file] { return file; });

    run(console, "echo \"$STC_FILE\"; echo \"$STC_DIR\"");
    EXPECT_TRUE(console.outputText().contains(QStringLiteral("\n%1\n").arg(file)));
    EXPECT_TRUE(console.outputText().contains(QStringLiteral("\n%1\n").arg(directory.path())));
}

TEST_F(ConsoleWidgetTest, CdUnderstandsTheVariablesOfTheEditor)
{
    ASSERT_TRUE(QDir(directory.path()).mkdir("articles"));
    const QString file = directory.filePath("articles/a.txt");
    console.setCurrentFileProvider([file] { return file; });

    run(console, "cd $STC_DIR");
    EXPECT_EQ(canonical(console.workingDirectory()), canonical(directory.filePath("articles")));
}

// ------------------------------------------------------------------ running state

TEST_F(ConsoleWidgetTest, StopsALongRunningCommand)
{
    QSignalSpy running(&console, &ConsoleWidget::runningChanged);
    console.runCommand("sleep 30");
    ASSERT_TRUE(waitUntilRunning(console));
    ASSERT_EQ(running.count(), 1);
    EXPECT_TRUE(running.at(0).at(0).toBool());

    QTest::qWait(150); // like in a terminal, ^C in the first milliseconds can reach the shell before it is ready for it
    console.interruptCommand();
    ASSERT_TRUE(waitUntilIdle(console, 5000));
    EXPECT_TRUE(console.outputText().contains("[interrupted]"));
    EXPECT_FALSE(running.last().at(0).toBool());
}

TEST_F(ConsoleWidgetTest, TheSecondInterruptKillsAStubbornCommand)
{
    // ignores SIGINT; the first interrupt changes nothing, the second one is SIGKILL
    console.runCommand("trap '' INT; sleep 30 & wait");
    ASSERT_TRUE(waitUntilRunning(console));
    QTest::qWait(300); // let the shell install the trap

    console.interruptCommand();
    QTest::qWait(300);
    EXPECT_TRUE(console.isRunning());

    console.interruptCommand();
    EXPECT_TRUE(waitUntilIdle(console, 5000));
}

TEST_F(ConsoleWidgetTest, TextTypedWhileACommandRunsGoesToItsInput)
{
    console.runCommand("read line; echo got:$line");
    ASSERT_TRUE(waitUntilRunning(console));

    console.runCommand("typed-text");
    ASSERT_TRUE(waitUntilIdle(console));
    EXPECT_TRUE(console.outputText().contains("got:typed-text"));
}

TEST_F(ConsoleWidgetTest, TypedTextIsNotAddedToTheHistoryAsACommand)
{
    console.runCommand("read line");
    ASSERT_TRUE(waitUntilRunning(console));
    console.runCommand("typed-text");
    ASSERT_TRUE(waitUntilIdle(console));
    EXPECT_EQ(console.history(), (QStringList{"read line"}));
}

TEST_F(ConsoleWidgetTest, FollowsTheOutputOnlyWhenTheViewIsAtTheBottom)
{
    run(console, "seq 1 400");
    QScrollBar* bar = outputOf(console)->verticalScrollBar();
    ASSERT_GT(bar->maximum(), 0);
    EXPECT_EQ(bar->value(), bar->maximum());

    bar->setValue(0);
    run(console, "seq 1 400");
    EXPECT_EQ(bar->value(), 0); // the user reads the beginning: not pulled away
}

TEST_F(ConsoleWidgetTest, DestroyingTheConsoleStopsTheCommand)
{
    auto temporary = std::make_unique<ConsoleWidget>();
    temporary->setWorkingDirectory(directory.path());
    temporary->runCommand(QStringLiteral("sleep 30"));
    ASSERT_TRUE(waitUntilRunning(*temporary));
    QElapsedTimer timer;
    timer.start();
    temporary.reset();
    EXPECT_LT(timer.elapsed(), 3000);
}

// ------------------------------------------------------------------ history

TEST_F(ConsoleWidgetTest, HistoryRemembersCommandsWithoutRepeatingTheLastOne)
{
    QSignalSpy changed(&console, &ConsoleWidget::historyChanged);
    run(console, "true");
    run(console, "true");
    run(console, "false");
    EXPECT_EQ(console.history(), (QStringList{"true", "false"}));
    EXPECT_EQ(changed.count(), 2);
}

TEST_F(ConsoleWidgetTest, HistoryCanBeRestored)
{
    console.setHistory({"ls", "make"});
    EXPECT_EQ(console.history(), (QStringList{"ls", "make"}));
}

// ------------------------------------------------------------------ the input line

TEST_F(ConsoleWidgetTest, UpAndDownWalkThroughTheHistoryAndKeepTheDraft)
{
    console.setHistory({"first", "second"});
    ConsoleInputEdit* input = inputOf(console);
    input->setPlainText("draft");

    QTest::keyClick(input, Qt::Key_Up);
    EXPECT_EQ(input->toPlainText(), "second");
    QTest::keyClick(input, Qt::Key_Up);
    EXPECT_EQ(input->toPlainText(), "first");
    QTest::keyClick(input, Qt::Key_Up); // the oldest: stays
    EXPECT_EQ(input->toPlainText(), "first");

    QTest::keyClick(input, Qt::Key_Down);
    EXPECT_EQ(input->toPlainText(), "second");
    QTest::keyClick(input, Qt::Key_Down);
    EXPECT_EQ(input->toPlainText(), "draft");
}

TEST_F(ConsoleWidgetTest, EnterRunsTheCommandAndClearsTheLine)
{
    ConsoleInputEdit* input = inputOf(console);
    input->setPlainText("echo from-the-line");
    QTest::keyClick(input, Qt::Key_Return);
    EXPECT_TRUE(input->toPlainText().isEmpty());
    ASSERT_TRUE(waitUntilIdle(console));
    EXPECT_TRUE(console.outputText().contains("from-the-line"));
}

TEST_F(ConsoleWidgetTest, TheLineNeverHasMoreThanOneRow)
{
    ConsoleInputEdit* input = inputOf(console);
    QTest::keyClicks(input, "echo a");
    QTest::keyClick(input, Qt::Key_Tab);
    EXPECT_EQ(input->document()->blockCount(), 1);
    EXPECT_FALSE(input->toPlainText().contains(QLatin1Char('\t')));
}

TEST_F(ConsoleWidgetTest, ControlCDropsTheTypedLineWhenNothingRuns)
{
    ConsoleInputEdit* input = inputOf(console);
    input->setPlainText("rm -rf nothing");
    QTest::keyClick(input, Qt::Key_C, Qt::ControlModifier);
    EXPECT_TRUE(input->toPlainText().isEmpty());
}

TEST_F(ConsoleWidgetTest, ControlLClearsTheOutput)
{
    run(console, "echo something");
    QTest::keyClick(inputOf(console), Qt::Key_L, Qt::ControlModifier);
    EXPECT_TRUE(console.outputText().isEmpty());
}

TEST_F(ConsoleWidgetTest, EscapeAsksToLeave)
{
    QSignalSpy leave(&console, &ConsoleWidget::leaveRequested);
    QTest::keyClick(inputOf(console), Qt::Key_Escape);
    EXPECT_EQ(leave.count(), 1);
}

TEST_F(ConsoleWidgetTest, PastedScriptBecomesASequenceOfCommands)
{
    ConsoleInputEdit* input = inputOf(console);
    QGuiApplication::clipboard()->setText("cd build\ncmake ..\n");
    input->paste();
    EXPECT_EQ(input->toPlainText(), "cd build; cmake ..");
}

TEST_F(ConsoleWidgetTest, TheInputLineIsColoredBySyntax)
{
    ConsoleInputEdit* input = inputOf(console);
    input->setPlainText("sh -c \"x y\" $HOME");

    const QList<QTextLayout::FormatRange> ranges = input->document()->firstBlock().layout()->formats();
    ASSERT_FALSE(ranges.isEmpty());
    auto colorAt = [&](int position) {
        for (const auto& range : ranges)
        {
            if (position >= range.start && position < range.start + range.length)
                return range.format.foreground().color();
        }
        return QColor();
    };
    const QColor command = colorAt(0);
    const QColor option = colorAt(3);
    const QColor string = colorAt(6);
    const QColor variable = colorAt(12);
    EXPECT_TRUE(command.isValid());
    EXPECT_TRUE(option.isValid());
    EXPECT_TRUE(string.isValid());
    EXPECT_TRUE(variable.isValid());
    EXPECT_NE(command, option);
    EXPECT_NE(option, string);
    EXPECT_NE(string, variable);
}

TEST_F(ConsoleWidgetTest, UnknownCommandHasADifferentColorThanAKnownOne)
{
    ConsoleInputEdit* input = inputOf(console);
    auto commandColor = [&](const QString& text) {
        input->setPlainText(text);
        const auto ranges = input->document()->firstBlock().layout()->formats();
        return ranges.isEmpty() ? QColor() : ranges.first().format.foreground().color();
    };
    const QColor known = commandColor("sh");
    const QColor unknown = commandColor("definitely-not-a-program-xyz");
    EXPECT_TRUE(known.isValid());
    EXPECT_TRUE(unknown.isValid());
    EXPECT_NE(known, unknown);
}

// ------------------------------------------------------------------ the margin: lines and time of a command

TEST_F(ConsoleWidgetTest, MarginCountsTheLinesOfTheOutput)
{
    run(console, "seq 1 5");
    const auto infos = console.commandInfos();
    ASSERT_EQ(infos.size(), 1);
    EXPECT_EQ(infos.first().lineCount, 5);
    EXPECT_FALSE(infos.first().running);
    EXPECT_TRUE(infos.first().showStats);
}

TEST_F(ConsoleWidgetTest, TheLastLineWithoutALineBreakIsCounted)
{
    run(console, R"(printf 'a\nb')");
    ASSERT_EQ(console.commandInfos().size(), 1);
    EXPECT_EQ(console.commandInfos().first().lineCount, 2);
}

TEST_F(ConsoleWidgetTest, CommandWithoutOutputHasZeroLines)
{
    run(console, "true");
    EXPECT_EQ(console.commandInfos().first().lineCount, 0);
}

TEST_F(ConsoleWidgetTest, EveryCommandHasItsOwnNumbers)
{
    run(console, "seq 1 3");
    run(console, "seq 1 10");
    const auto infos = console.commandInfos();
    ASSERT_EQ(infos.size(), 2);
    EXPECT_EQ(infos.at(0).lineCount, 3);
    EXPECT_EQ(infos.at(1).lineCount, 10);
}

TEST_F(ConsoleWidgetTest, StatusLineAndTypedTextAreNotCountedAsOutput)
{
    run(console, "exit 2");
    EXPECT_EQ(console.commandInfos().first().lineCount, 0); // "[exit code 2]" is not the output of the command
}

TEST_F(ConsoleWidgetTest, MarginShowsTheDurationAlsoWhileTheCommandRuns)
{
    console.runCommand("sleep 1");
    ASSERT_TRUE(waitUntilRunning(console));
    QTest::qWait(500);
    auto infos = console.commandInfos();
    ASSERT_EQ(infos.size(), 1);
    EXPECT_TRUE(infos.first().running);
    EXPECT_GE(infos.first().durationMs, 300);

    ASSERT_TRUE(waitUntilIdle(console));
    infos = console.commandInfos();
    EXPECT_FALSE(infos.first().running);
    EXPECT_GE(infos.first().durationMs, 900);
    EXPECT_LT(infos.first().durationMs, 5000);
}

TEST_F(ConsoleWidgetTest, CdHasNoNumbersInTheMargin)
{
    ASSERT_TRUE(QDir(directory.path()).mkdir("sub"));
    run(console, "cd sub");
    ASSERT_EQ(console.commandInfos().size(), 1);
    EXPECT_FALSE(console.commandInfos().first().showStats);
}

TEST_F(ConsoleWidgetTest, MarginTakesPlaceOnlyAfterTheFirstCommand)
{
    console.show();
    QCoreApplication::processEvents();
    EXPECT_EQ(outputOf(console)->viewport()->x(), outputOf(console)->contentsRect().left());
    run(console, "true");
    QCoreApplication::processEvents();
    EXPECT_GT(outputOf(console)->viewport()->x(), outputOf(console)->contentsRect().left());
}

TEST_F(ConsoleWidgetTest, MarginRemembersTheDirectoryOfTheCommand)
{
    run(console, "true");
    EXPECT_EQ(canonical(console.commandInfos().first().workingDirectory), canonical(directory.path()));
}

// ------------------------------------------------------------------ clearing

TEST_F(ConsoleWidgetTest, ClearButtonEmptiesTheOutput)
{
    run(console, "echo something");
    ASSERT_FALSE(console.outputText().isEmpty());

    QToolButton* clearButton = nullptr;
    for (QToolButton* button : console.findChildren<QToolButton*>())
    {
        if (button->toolTip().contains("Clear the console"))
            clearButton = button;
    }
    ASSERT_NE(clearButton, nullptr);
    clearButton->click();
    EXPECT_TRUE(console.outputText().isEmpty());
}

TEST_F(ConsoleWidgetTest, ClearRequestFromTheOutputEmptiesIt)
{
    run(console, "echo something");
    emit outputOf(console)->clearRequested();
    EXPECT_TRUE(console.outputText().isEmpty());
}

TEST_F(ConsoleWidgetTest, OutputOfARunningCommandContinuesAfterClearing)
{
    console.runCommand("echo before; sleep 0.5; echo after");
    ASSERT_TRUE(waitUntilRunning(console));
    QTest::qWait(150);
    console.clearOutput();
    ASSERT_TRUE(waitUntilIdle(console));
    EXPECT_TRUE(console.outputText().contains("after"));
    EXPECT_FALSE(console.outputText().contains("before"));
}

// ------------------------------------------------------------------ directories in listings

class ConsoleWidgetListingTest : public ConsoleWidgetTest
{
protected:
    ConsoleWidgetListingTest()
    {
        QDir dir(directory.path());
        dir.mkdir("sub");
        QFile(dir.filePath("file.txt")).open(QIODevice::WriteOnly);
        QFile(dir.filePath("sub/inner.txt")).open(QIODevice::WriteOnly);
    }

    /// The format of the first character of `text` in the result of the command (not in the echo of the command).
    QTextCharFormat formatOf(const QString& text)
    {
        QTextDocument* document = outputOf(console)->document();
        const QTextCursor found = document->find(text, QTextCursor(document->firstBlock().next()));
        if (found.isNull())
            return {};
        QTextCursor cursor(document);
        cursor.setPosition(found.selectionStart() + 1);
        return cursor.charFormat();
    }

    bool isDirectoryColored(const QString& text)
    {
        return isDirectory(formatOf(text));
    }

    static bool isDirectory(const QTextCharFormat& format)
    {
        return format.fontWeight() == QFont::Bold && format.hasProperty(QTextFormat::ForegroundBrush);
    }

    /// The position of the start of the line which is exactly `line` (not the echo of the command), -1 when there is none
    int positionInLine(const QString& line)
    {
        QTextDocument* document = outputOf(console)->document();
        for (QTextBlock block = document->firstBlock().next(); block.isValid(); block = block.next())
        {
            if (block.text() == line)
                return block.position();
        }
        return -1;
    }

    bool isLineColored(const QString& line)
    {
        const int position = positionInLine(line);
        if (position < 0)
            return false;
        QTextCursor cursor(outputOf(console)->document());
        cursor.setPosition(position + 1); // the format of the first character is the one before the cursor
        return isDirectory(cursor.charFormat());
    }
};

TEST_F(ConsoleWidgetListingTest, LsPaintsDirectoriesAndNotFiles)
{
    run(console, "ls");
    EXPECT_TRUE(isDirectoryColored("sub"));
    EXPECT_FALSE(isDirectoryColored("file.txt"));
}

TEST_F(ConsoleWidgetListingTest, FindPaintsDirectoriesAndNotFiles)
{
    run(console, "find .");
    EXPECT_GE(positionInLine("./sub"), 0);
    EXPECT_TRUE(isLineColored("./sub"));
    EXPECT_TRUE(isLineColored("."));
    EXPECT_FALSE(isLineColored("./file.txt"));
    EXPECT_FALSE(isLineColored("./sub/inner.txt"));
}

TEST_F(ConsoleWidgetListingTest, LsLongPaintsOnlyTheNameOfADirectory)
{
    run(console, "ls -l");
    EXPECT_TRUE(isDirectoryColored("sub"));
    EXPECT_FALSE(isDirectoryColored("drwx")); // the permissions of the row are not painted
    EXPECT_FALSE(isDirectoryColored("file.txt"));
}

TEST_F(ConsoleWidgetListingTest, DirectoriesAreFoundInAPipeline)
{
    run(console, "ls | sort");
    EXPECT_TRUE(isDirectoryColored("sub"));
}

TEST_F(ConsoleWidgetListingTest, OtherCommandsAreNotColored)
{
    run(console, "printf 'sub\\n'");
    EXPECT_FALSE(isDirectoryColored("sub"));
}

TEST_F(ConsoleWidgetListingTest, OutputWithItsOwnColorsIsLeftAlone)
{
    // the same name, but the program colored it: the program wins
    run(console, R"(ls | sed 's/^/\x1b[32m/;s/$/\x1b[0m/')");
    const QTextCharFormat format = formatOf("sub");
    ASSERT_TRUE(format.hasProperty(QTextFormat::ForegroundBrush));
    EXPECT_NE(format.fontWeight(), QFont::Bold);
}

// ------------------------------------------------------------------ double click on a path

class ConsoleWidgetDoubleClickTest : public ConsoleWidgetListingTest
{
protected:
    ConsoleWidgetDoubleClickTest()
    {
        console.show();
        QCoreApplication::processEvents();
    }

    /// Double click on the first occurrence of `text` in the result of the last command
    void doubleClickOnLine(const QString& line, int offset = 3)
    {
        ConsoleOutputEdit* output = outputOf(console);
        const int position = positionInLine(line);
        ASSERT_GE(position, 0) << qPrintable(line);
        QTextCursor at(output->document());
        at.setPosition(position + offset);
        QTest::mouseDClick(output->viewport(), Qt::LeftButton, Qt::NoModifier, output->cursorRect(at).center());
    }

    void doubleClickOn(const QString& text)
    {
        ConsoleOutputEdit* output = outputOf(console);
        QTextDocument* document = output->document();
        const QTextCursor found = document->find(text, QTextCursor(document->firstBlock().next()));
        ASSERT_FALSE(found.isNull()) << qPrintable(text);
        QTextCursor at(document);
        at.setPosition(found.selectionStart() + 1);
        const QPoint point = output->cursorRect(at).center();
        QTest::mouseDClick(output->viewport(), Qt::LeftButton, Qt::NoModifier, point);
    }
};

TEST_F(ConsoleWidgetDoubleClickTest, NameInAListingOpensTheDirectory)
{
    run(console, "ls");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("sub");
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, PathOfFindOpensTheDirectory)
{
    run(console, "find .");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOnLine("./sub");
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, PathOfAFileOpensTheDirectoryWhichContainsIt)
{
    run(console, "find sub");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("sub/inner.txt");
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, RowOfLsLongOpensTheDirectory)
{
    run(console, "ls -l");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("drwx");
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, PathWithALineNumberOpensTheDirectory)
{
    run(console, "echo sub/inner.txt:12:5: error: oops");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("sub/inner.txt");
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, RelativePathsAreRelativeToWhereTheCommandRan)
{
    run(console, "find .");
    run(console, "cd sub");
    run(console, "ls"); // inner.txt: relative to sub
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);

    // in the output of `find .` (the first command) paths are relative to the directory, which was current then
    doubleClickOnLine("./sub", 4);
    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(canonical(opened.at(0).at(0).toString()), canonical(directory.filePath("sub")));
}

TEST_F(ConsoleWidgetDoubleClickTest, WordOfASentenceIsJustSelected)
{
    run(console, "echo sub is a word"); // "sub" exists, but this is not a path
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("sub is");
    EXPECT_EQ(opened.count(), 0);
    EXPECT_EQ(outputOf(console)->textCursor().selectedText(), "sub");
}

TEST_F(ConsoleWidgetDoubleClickTest, ThingWhichDoesNotExistIsJustSelected)
{
    run(console, "echo no/such/path");
    QSignalSpy opened(&console, &ConsoleWidget::openDirectoryRequested);
    doubleClickOn("no/such/path");
    EXPECT_EQ(opened.count(), 0);
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
