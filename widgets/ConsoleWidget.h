#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringConverter>
#include <QStringList>
#include <QTextCharFormat>
#include <QWidget>
#include <functional>

#include "utils/AnsiTextWriter.h"

class ConsoleInputEdit;
class QLabel;
class QPlainTextEdit;
class QProcess;
class QToolButton;

/// A small console built into the editor: the output of the commands (with colors, see `AnsiTextWriter`) above
/// and one colored line (`ConsoleInputEdit`) in which the commands are typed.
///
/// It is not a terminal emulator: every line is run by the shell (`/bin/sh -c`, `cmd.exe /c` on Windows)
/// as a separate process, without a pseudo terminal. So `g++`, `cmake`, `git`, `ls`, pipes and redirections work,
/// `cd` is handled by the console (the directory is kept between the commands), and the text typed while a command
/// runs goes to its standard input. Programs which draw on the screen (`vim`, `top`, `htop`, a password prompt of `sudo`)
/// do not work.
///
/// The commands see the variables `STC_FILE` (the file open in the editor) and `STC_DIR` (its directory).
class ConsoleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ConsoleWidget(QWidget* parent = nullptr);
    ~ConsoleWidget() override;

    QString workingDirectory() const { return workingDirectory_; }
    /// A directory which does not exist is ignored.
    void setWorkingDirectory(const QString& directory);

    /// Gives the file open in the editor (for `STC_FILE` / `STC_DIR`); it is asked each time a command starts.
    void setCurrentFileProvider(std::function<QString()> provider);

    void setHistory(const QStringList& commands);
    QStringList history() const;

    bool isRunning() const { return process_ != nullptr; }
    /// The text of the output (without the formatting).
    QString outputText() const;

    /// Does what Enter does after typing `line`.
    void runCommand(const QString& line);
    void clearOutput();
    void focusInput();
    /// The first call stops the running command like Ctrl+C does, the next one kills it.
    void interruptCommand();

signals:
    void runningChanged(bool running);
    void workingDirectoryChanged(const QString& directory);
    /// The history changed (a command was added); for saving it.
    void historyChanged(const QStringList& history);
    /// The command ended; the exit code is -1 when it did not start or was terminated.
    void commandFinished(int exitCode);
    /// Escape was pressed in the input line.
    void leaveRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void submit(const QString& line);
    void startProcess(const QString& line);
    void changeDirectory(const QString& argument);
    QProcessEnvironment processEnvironment() const;

    void readAvailable();
    void appendOutput(QString text);
    void appendStatus(const QString& text, bool error);
    void echoCommand(const QString& line);
    void echoStandardInput(const QString& text);
    void ensureAtLineStart();
    void eraseLastLine();
    void insertText(const QString& text, const QTextCharFormat& format);
    bool darkBackground() const;

    void onProcessFinished(int exitCode, int exitStatus);
    void onProcessFailedToStart(const QString& reason);
    void updateRunningUi();
    void updatePrompt();
    QString shortWorkingDirectory() const;
    void retranslate();

    QPlainTextEdit* output_ = nullptr;
    ConsoleInputEdit* input_ = nullptr;
    QLabel* prompt_ = nullptr;
    QToolButton* stopButton_ = nullptr;

    /// One process at a time. Its standard error is merged into the standard output (as in a terminal), so the lines
    /// stay in the order in which the program wrote them.
    QProcess* process_ = nullptr;
    QStringDecoder decoder_{QStringConverter::System}; ///< a character can be split between two reads
    AnsiTextWriter writer_;
    bool pendingCarriageReturn_ = false; ///< the last chunk ended with '\r', which may be a part of "\r\n"
    int interruptCount_ = 0;

    QString workingDirectory_;
    QString previousDirectory_;
    std::function<QString()> currentFile_;
};
