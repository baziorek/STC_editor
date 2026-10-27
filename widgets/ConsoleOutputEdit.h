#pragma once

#include <QList>
#include <QPlainTextEdit>
#include <QString>
#include <memory>

class QContextMenuEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QTextBlock;

/// What the left margin of the console shows for one command.
struct ConsoleCommandInfo
{
    QString workingDirectory; ///< where the command was started: relative paths in its output are relative to it
    int lineCount = 0;        ///< lines of the output of the command
    qint64 durationMs = 0;
    bool running = false;
    bool showStats = true;    ///< false for what the console does itself (`cd`): the margin stays empty
};

/// The output of the console: a read-only text with
///  - a margin on the left, in which every command has the number of lines of its output and its duration,
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

    /// The directory for the paths which do not follow any command (when the output was cleared meanwhile).
    void setDefaultDirectory(const QString& directory) { defaultDirectory_ = directory; }

    /// The commands whose line is still in the output, the oldest first.
    QList<ConsoleCommandInfo> commandInfos() const;

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

    QWidget* margin_ = nullptr;
    QString defaultDirectory_;
    bool hasCommands_ = false;
};
