#pragma once

#include <QList>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <optional>

/// The part of the console (`ConsoleWidget`) which does not need any widget, so it can be tested:
/// splitting of the line into the highlighted tokens, splitting into words (for the built-in `cd`),
/// recognizing the commands handled by the console itself.
///
/// The syntax is the one of the POSIX `sh` (`/bin/sh -c` runs the line); on Windows the line is run by `cmd.exe`,
/// whose syntax is close enough for the highlighting (quotes, `|`, `&&`, `>`, options), except that
/// the backslash is an ordinary character there.
namespace ConsoleShell
{
enum class TokenKind
{
    Command,    ///< the first word of a simple command: the program (or built-in) which is run
    Assignment, ///< `NAME=value` in front of the command
    Argument,   ///< any other word
    Option,     ///< `-x`, `--long`
    String,     ///< `'...'` or `"..."`, also the one which is not closed yet
    Variable,   ///< `$NAME`, `${NAME}`, `$?`, ...
    Operator,   ///< `|`, `||`, `&`, `&&`, `;`, `>`, `>>`, `<`, `2>`, `(`, `)`, `$(`
    Comment     ///< `#` up to the end of the line
};

struct Token
{
    TokenKind kind;
    qsizetype start;
    qsizetype length;
    bool operator==(const Token&) const = default;
};

/// Splits the line into tokens (the white space between them is not a token). The tokens are sorted
/// and do not overlap; the text of the line which is not covered by any token is white space.
QList<Token> tokenize(const QString& line);

/// Splits the line into words like the shell does: quotes are removed, `\x` is `x` (not on Windows).
/// With `environment` the `$NAME` and `${NAME}` outside of the single quotes are replaced by the values
/// (an unknown variable is an empty text). A `#` comment is not recognized here (see `parseBuiltin()`).
QStringList splitWords(const QString& line, const QProcessEnvironment* environment = nullptr);

/// The directory to which `cd argument` leads: empty argument and `~` is `homeDirectory`,
/// `-` is `previousDirectory` (can be empty - then an empty text is returned), `~/x` is in the home directory,
/// a relative path starts in `currentDirectory`. The result is a clean path; whether it exists is not checked.
QString resolveCdTarget(const QString& currentDirectory, const QString& argument,
                        const QString& homeDirectory, const QString& previousDirectory);

enum class BuiltinKind
{
    None,            ///< the line is run by the shell
    ChangeDirectory, ///< `cd`: a child process cannot change the directory of the console, so the console does it
    Clear            ///< `clear`, `cls`
};

struct Builtin
{
    BuiltinKind kind = BuiltinKind::None;
    QString argument; ///< `cd`: the directory as typed, with the variables expanded (empty for a bare `cd`)
};

/// Recognizes the lines handled by the console itself: `cd`, `cd dir`, `clear`, `cls` (nothing else on the line,
/// no `&&`, no pipe - `cd build && make` is run by the shell, where `cd` works for the rest of that line).
Builtin parseBuiltin(const QString& line, const QProcessEnvironment& environment);

/// What the first word of a command is, for the color in the input line.
enum class CommandKind
{
    ConsoleBuiltin, ///< handled by the console or a well known shell built-in (`cd`, `echo`, `export`, `if`, ...)
    Executable,     ///< a program found in `PATH` or the executable file given by a path
    Unknown
};

/// Looks for the command like the shell would: built-ins, then a path (relative to `workingDirectory`),
/// then `PATH`. Touches the disk, so the caller is expected to cache the answers.
CommandKind classifyCommand(const QString& word, const QString& workingDirectory);

/// A text pasted into the one-line input as a sequence of commands: empty lines are dropped, the lines are
/// separated with `; ` (a line ending with a backslash is continued in the same command).
QString joinPastedLines(const QString& text);

/// The time of a command for the left margin of the console: "850 ms", "3.2 s", "2m 05s", "1h 02m".
QString formatDuration(qint64 milliseconds);

/// Whether the command line runs a program which lists files (`ls`, `find`, `fd`, `dir`, ...), so its output
/// is worth looking at for directories (see `ListingClassifier`).
bool isListingCommand(const QString& line);

/// `path` as an absolute clean path: `~` is the home directory, a relative path starts in `baseDirectory`.
QString resolvePath(const QString& baseDirectory, const QString& path);

/// What a double click at `column` of one line of the output could mean as a path, the most probable first
/// (not checked on the disk): the word under the cursor when it looks like a path (`src/a.cpp:12:5` also
/// without `:12:5`), the name of a row of `ls -l`, the whole line (a name in a listing, spaces allowed).
QStringList pathCandidatesAt(const QString& line, qsizetype column);

/// Finds the directories in the output of `ls`, `ls -l`, `ls -R`, `find` and `dir`, line by line.
/// No terminal means no colors from `ls`, and `find` has none at all, so the lines are checked on the disk:
/// the line is a name (a path) relative to the directory in which the command was started.
class ListingClassifier
{
public:
    struct Range
    {
        qsizetype start;
        qsizetype length;
        bool operator==(const Range&) const = default;
    };

    explicit ListingClassifier(QString baseDirectory = {}) : baseDirectory_(std::move(baseDirectory)) {}

    /// The part of the line which names a directory; nothing for any other line. Lines have to come in the
    /// order of the output: `ls -R` and `ls a b` print `dir:` before the names in it.
    std::optional<Range> directoryRange(const QString& line);

private:
    QString baseDirectory_;
    QString context_; ///< the directory from the last `dir:` header, relative to the base
};
} // namespace ConsoleShell
