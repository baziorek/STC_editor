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
#include <QTest>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QThread>
#include <memory>

#include "widgets/ConsoleInputEdit.h"
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

QPlainTextEdit* outputOf(ConsoleWidget& console)
{
    return console.findChild<QPlainTextEdit*>(); // the first one is the output (the input line comes later)
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

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
