#pragma once

#include <QPlainTextEdit>
#include <QString>
#include <QStringList>

class ConsoleSyntaxHighlighter;
class QKeyEvent;
class QMimeData;

/// The one line of the console in which commands are typed: colored (see `ConsoleSyntaxHighlighter`),
/// with the history of the commands (Up / Down).
///
/// Enter emits `commandSubmitted()` and clears the line. Ctrl+C (without a selection), Ctrl+L, Ctrl+D (in an empty line),
/// Ctrl+Up / Ctrl+Down and Escape are only reported - the owner decides what they mean.
class ConsoleInputEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ConsoleInputEdit(QWidget* parent = nullptr);

    /// The oldest command first.
    void setHistory(const QStringList& commands);
    QStringList history() const { return history_; }
    /// Adds a command to the end of the history (not an empty one, not the same as the last one).
    /// Returns whether the history changed.
    bool addToHistory(const QString& command);

    void setWorkingDirectory(const QString& directory);

signals:
    void commandSubmitted(const QString& command);
    void interruptRequested();
    void clearOutputRequested();
    void endOfInputRequested();
    void leaveRequested();
    /// Ctrl+Up / Ctrl+Down: the output scrolls to the previous / next command
    void previousCommandRequested();
    void nextCommandRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void insertFromMimeData(const QMimeData* source) override;
    void changeEvent(QEvent* event) override;

private:
    void navigateHistory(int direction);
    void resetHistoryPosition();
    void updateHeight();
    void updateColorsForPalette();

    ConsoleSyntaxHighlighter* highlighter_ = nullptr;
    QStringList history_;
    qsizetype historyIndex_ = 0; ///< == history_.size() while a new command (the draft) is edited
    QString draft_;              ///< the text typed before Up was pressed
};
