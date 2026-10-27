#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QPlainTextEdit>
#include <QString>
#include <QTextBlock>
#include <memory>

class QContextMenuEvent;
class QEvent;
class QMouseEvent;
class QWidget;
class QPaintEvent;
class QResizeEvent;

/// What the left margin of the console shows for one command.
struct ConsoleCommandInfo
{
    QString workingDirectory; ///< where the command was started: relative paths in its output are relative to it
    int lineCount = 0;        ///< lines of the output of the command (the tooltip of the margin)
    int nextLineNumber = 1;   ///< the number of the next line of the output, which the margin shows
    qint64 durationMs = 0;
    bool running = false;
    bool showStats = true;    ///< false for what the console does itself (`cd`): the margin stays empty
};

/// The output of the console: a read-only text with
///  - a margin on the left: the duration of every command (on its line, to the left) and the numbers of the lines
///    of its output (one by one, to the right),
///  - a double click in the margin selects the whole output of the command, a triple click the output with the command,
///  - jumps between the commands (`jumpToPreviousCommand()`, `jumpToNextCommand()`),
///  - a double click on a path opening it (`pathActivated`),
///  - "Clear" in the context menu.
class ConsoleOutputEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ConsoleOutputEdit(QWidget* parent = nullptr);
    ~ConsoleOutputEdit() override;

    /// Marks the line before the last one (the echo of the command, which ends with a line break) as the line of the command.
    /// The numbers of `info` can change later, `refreshMargin()` shows the new ones.
    void markCommandLine(const std::shared_ptr<ConsoleCommandInfo>& info);
    void refreshMargin();

    /// Gives the numbers 1, 2, 3, ... (of the output of the command) to the lines from `firstNewBlock` to the end which
    /// have none yet. The empty last line is the place of the next text, it has no number yet.
    void numberOutputLines(const QTextBlock& firstNewBlock, ConsoleCommandInfo& info);
    /// The line before the last one (a status line, a typed line sent to the program) is not a line of the output:
    /// no number, and it is not counted.
    void markAnnotationLine();
    /// The number which the margin shows at the line; 0 when there is none.
    int lineNumberOf(const QTextBlock& block) const;

    /// The first line which is (at least partly) visible.
    QTextBlock topVisibleBlock() const { return firstVisibleBlock(); }

    /// The margin (for the tests).
    QWidget* marginWidget() const { return margin_; }

    /// The directory for the paths which do not follow any command (when the output was cleared meanwhile).
    void setDefaultDirectory(const QString& directory) { defaultDirectory_ = directory; }

    /// The commands whose line is still in the output, the oldest first.
    QList<ConsoleCommandInfo> commandInfos() const;

public slots:
    /// Scrolls to the line of the previous (next) command, as seen from the top of the window; after the last one to the end.
    void jumpToPreviousCommand();
    void jumpToNextCommand();

signals:
    /// A double click on an existing path; the argument is its absolute path (of a file, or of a directory).
    void pathActivated(const QString& absolutePath);
    void clearRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class ConsoleMargin;

    int marginWidth() const;
    void updateMarginWidth();
    void updateMarginArea(const QRect& rect, int dy);
    void paintMargin(QPaintEvent* event);
    QString marginToolTipAt(int y) const;
    QString directoryOfCommandAt(const QTextBlock& block) const;

    void marginDoubleClicked(int y);
    void marginPressed(int y);
    QTextBlock blockAtMargin(int y) const;
    void selectCommand(const QTextBlock& blockOfCommand, bool withCommandLine);
    QTextBlock commandBlockOf(QTextBlock block) const;
    QTextBlock nextCommandBlock(QTextBlock block) const;
    void scrollBlockToTop(const QTextBlock& block);

    QWidget* margin_ = nullptr;
    QString defaultDirectory_;
    bool hasCommands_ = false;
    QElapsedTimer tripleClick_; ///< runs after a double click in the margin: the next press is a triple click
};
