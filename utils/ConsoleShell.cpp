#include "ConsoleShell.h"

#include <QChar>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStringView>
#include <initializer_list>

namespace ConsoleShell
{
namespace
{
/// A set of QStrings from plain literals. (The `u"..."_s` literals of Qt::StringLiterals need Qt 6.4,
/// while Ubuntu 22.04 - the CI of the Linux build - has Qt 6.2.)
QSet<QString> wordSet(std::initializer_list<const char*> words)
{
    QSet<QString> set;
    set.reserve(static_cast<qsizetype>(words.size()));
    for (const char* word : words)
        set.insert(QString::fromLatin1(word));
    return set;
}

#ifdef Q_OS_WIN
constexpr bool kBackslashEscapes = false; // `C:\dir\file`
#else
constexpr bool kBackslashEscapes = true;
#endif

bool isOperatorChar(QChar c)
{
    switch (c.unicode())
    {
    case '|':
    case '&':
    case ';':
    case '<':
    case '>':
    case '(':
    case ')':
        return true;
    default:
        return false;
    }
}

bool isNameChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

/// Index after the variable which starts at `line[at] == '$'`; `at` itself when it is not a variable.
qsizetype endOfVariable(const QString& line, qsizetype at)
{
    const qsizetype size = line.size();
    qsizetype j = at + 1;
    if (j >= size)
        return at;
    if (line[j] == QLatin1Char('{'))
    {
        const qsizetype close = line.indexOf(QLatin1Char('}'), j);
        return close < 0 ? size : close + 1; // not closed yet: the variable goes on to the end
    }
    if (line[j].isLetter() || line[j] == QLatin1Char('_'))
    {
        while (j < size && isNameChar(line[j]))
            ++j;
        return j;
    }
    if (line[j].isDigit() || QStringView(u"?$!#@*").contains(line[j]))
        return j + 1;
    return at;
}

/// Index after the quoted text which starts at `line[at]` (a quote); the end of the line when it is not closed.
qsizetype endOfQuoted(const QString& line, qsizetype at)
{
    const QChar quote = line[at];
    const qsizetype size = line.size();
    qsizetype i = at + 1;
    while (i < size && line[i] != quote)
    {
        if (quote == QLatin1Char('"') && kBackslashEscapes && line[i] == QLatin1Char('\\'))
            ++i; // the escaped character
        ++i;
    }
    return i < size ? i + 1 : size;
}

bool isAssignment(QStringView word)
{
    const qsizetype equals = word.indexOf(QLatin1Char('='));
    if (equals <= 0 || word[0].isDigit())
        return false;
    for (qsizetype i = 0; i < equals; ++i)
    {
        if (!isNameChar(word[i]))
            return false;
    }
    return true;
}

/// Words after which the next word is a command again (`sudo make`, `if test ...`, `time ls`).
bool keepsCommandPosition(QStringView word)
{
    static const QSet<QString> prefixes =
        wordSet({"if", "then", "else", "elif", "do", "while", "until", "!", "time", "sudo", "exec", "nohup", "command"});
    return prefixes.contains(word.toString());
}

/// Words handled by the shell itself (they are not programs on the disk, or not always).
bool isShellBuiltin(const QString& word)
{
    static const QSet<QString> builtins = wordSet({
        "cd",    "clear", "cls",   "echo",    "export", "unset",  "source", ".",
        "alias", "set",   "read",  "pwd",     "exit",   "exec",   "eval",    "type",
        "test",  "[",     "if",    "then",     "else",   "elif",   "fi",      "for",
        "in",    "do",    "done",  "while",    "until",  "case",   "esac",    "time",
        "wait",  "ulimit", "umask", "history", "command", "dir",   "copy",    "del",
        "move",  "ren",   "mkdir", "rmdir",   "md",     "rd",     "chdir"});
    return builtins.contains(word);
}

struct ListingRow
{
    bool directory;
    qsizetype nameStart;
};

/// A row of `ls -l` (also `--time-style=long-iso`) or of the `dir` of Windows.
std::optional<ListingRow> parseListingRow(const QString& line)
{
    static const QRegularExpression unixLong(
        QStringLiteral(R"(^([-dlbcpsDw])[rwxsStT?-]{9}[.+@]?\s+\d+\s+\S+\s+\S+\s+\S+\s+)"
                       R"((?:\d{4}-\d\d-\d\d\s+\S+|\S+\s+\d+\s+\S+)\s+(.+)$)"));
    static const QRegularExpression windowsDir(QStringLiteral(R"(^\S+\s+\S+(?:\s+[AaPp][Mm])?\s+(<DIR>|[\d.,\xA0]+)\s+(.+)$)"));

    if (const QRegularExpressionMatch match = unixLong.match(line); match.hasMatch())
    {
        const QChar type = match.captured(1).at(0);
        // a link ("name -> target") is not painted: whether it leads to a directory is not the point of the row
        return ListingRow{type == QLatin1Char('d'), match.capturedStart(2)};
    }
    if (const QRegularExpressionMatch match = windowsDir.match(line); match.hasMatch())
        return ListingRow{match.captured(1) == QLatin1String("<DIR>"), match.capturedStart(2)};
    return std::nullopt;
}
} // namespace

QList<Token> tokenize(const QString& line)
{
    QList<Token> tokens;
    const qsizetype size = line.size();
    bool expectCommand = true;
    qsizetype i = 0;

    while (i < size)
    {
        const QChar c = line[i];
        if (c.isSpace())
        {
            ++i;
            continue;
        }

        if (c == QLatin1Char('#')) // a token never starts in the middle of a word, so this is the start of a word
        {
            tokens.append({TokenKind::Comment, i, size - i});
            break;
        }

        if (c == QLatin1Char('$') && i + 1 < size && line[i + 1] == QLatin1Char('('))
        {
            tokens.append({TokenKind::Operator, i, 2});
            i += 2;
            expectCommand = true;
            continue;
        }

        // `2>file`: the number is a part of the operator
        qsizetype digitsEnd = i;
        while (digitsEnd < size && line[digitsEnd].isDigit())
            ++digitsEnd;
        const bool redirectWithNumber = digitsEnd > i && digitsEnd < size
                                        && (line[digitsEnd] == QLatin1Char('>') || line[digitsEnd] == QLatin1Char('<'));

        if (isOperatorChar(c) || redirectWithNumber)
        {
            const qsizetype start = i;
            i = digitsEnd;
            while (i < size && isOperatorChar(line[i]))
                ++i;
            const QStringView text = QStringView(line).mid(start, i - start);
            tokens.append({TokenKind::Operator, start, i - start});
            // after `>` comes the name of a file; after `|`, `&&`, `;`, `(` comes a command
            if (!text.contains(QLatin1Char('>')) && !text.contains(QLatin1Char('<')))
                expectCommand = true;
            continue;
        }

        if (c == QLatin1Char('$'))
        {
            const qsizetype end = endOfVariable(line, i);
            if (end > i)
            {
                tokens.append({TokenKind::Variable, i, end - i});
                i = end;
                expectCommand = false;
                continue;
            }
        }

        // a word; it may contain quoted parts (`--name="a b"`), and a variable ends it (`dir/$NAME`)
        const qsizetype start = i;
        const bool startsWithQuote = (c == QLatin1Char('\'') || c == QLatin1Char('"'));
        while (i < size)
        {
            const QChar ch = line[i];
            if (ch.isSpace() || isOperatorChar(ch) || (ch == QLatin1Char('$') && i > start))
                break;
            if (kBackslashEscapes && ch == QLatin1Char('\\'))
            {
                i = qMin(i + 2, size);
                continue;
            }
            if (ch == QLatin1Char('\'') || ch == QLatin1Char('"'))
            {
                i = endOfQuoted(line, i);
                continue;
            }
            ++i;
        }
        if (i == start) // cannot happen, but an endless loop is worse than a wrong color
            i = start + 1;

        const QStringView word = QStringView(line).mid(start, i - start);
        TokenKind kind;
        if (expectCommand)
        {
            if (isAssignment(word))
            {
                kind = TokenKind::Assignment; // the command is still to come
            }
            else
            {
                kind = TokenKind::Command;
                expectCommand = keepsCommandPosition(word);
            }
        }
        else if (startsWithQuote)
        {
            kind = TokenKind::String;
        }
        else if (word.size() > 1 && word[0] == QLatin1Char('-'))
        {
            kind = TokenKind::Option;
        }
        else
        {
            kind = TokenKind::Argument;
        }
        tokens.append({kind, start, i - start});
    }
    return tokens;
}

QStringList splitWords(const QString& line, const QProcessEnvironment* environment)
{
    QStringList words;
    QString word;
    bool inWord = false; // `""` is a word although it is empty
    QChar quote;         // the open quote, or a null character
    const qsizetype size = line.size();

    auto endWord = [&] {
        if (inWord)
            words << word;
        word.clear();
        inWord = false;
    };

    for (qsizetype i = 0; i < size; ++i)
    {
        const QChar c = line[i];

        if (quote.isNull() && c.isSpace())
        {
            endWord();
            continue;
        }

        if (c == QLatin1Char('\'') && (quote.isNull() || quote == c))
        {
            quote = quote.isNull() ? c : QChar();
            inWord = true;
            continue;
        }
        if (c == QLatin1Char('"') && (quote.isNull() || quote == c))
        {
            quote = quote.isNull() ? c : QChar();
            inWord = true;
            continue;
        }

        if (kBackslashEscapes && c == QLatin1Char('\\') && quote != QLatin1Char('\'') && i + 1 < size)
        {
            const QChar next = line[i + 1];
            // inside "..." only these are escaped, the rest keeps the backslash
            if (quote.isNull() || next == QLatin1Char('"') || next == QLatin1Char('\\') || next == QLatin1Char('$')
                || next == QLatin1Char('`'))
            {
                word += next;
                inWord = true;
                ++i;
                continue;
            }
        }

        if (environment && c == QLatin1Char('$') && quote != QLatin1Char('\''))
        {
            const qsizetype end = endOfVariable(line, i);
            if (end > i)
            {
                QString name = line.mid(i + 1, end - i - 1);
                if (name.startsWith(QLatin1Char('{')))
                    name = name.mid(1, name.size() - (name.endsWith(QLatin1Char('}')) ? 2 : 1));
                word += environment->value(name);
                inWord = true;
                i = end - 1;
                continue;
            }
        }

        word += c;
        inWord = true;
    }
    endWord();
    return words;
}

QString resolveCdTarget(const QString& currentDirectory, const QString& argument,
                        const QString& homeDirectory, const QString& previousDirectory)
{
    if (argument.isEmpty() || argument == QLatin1String("~"))
        return homeDirectory;
    if (argument == QLatin1String("-"))
        return previousDirectory;

    QString path = argument;
    if (path.startsWith(QLatin1String("~/")) || path.startsWith(QLatin1String("~\\")))
        path = homeDirectory + path.mid(1);

    if (QDir::isAbsolutePath(path))
        return QDir::cleanPath(path);
    return QDir::cleanPath(QDir(currentDirectory).filePath(path));
}

Builtin parseBuiltin(const QString& line, const QProcessEnvironment& environment)
{
    const QList<Token> tokens = tokenize(line);
    QString code = line;
    for (const Token& token : tokens)
    {
        if (token.kind == TokenKind::Operator || token.kind == TokenKind::Assignment)
            return {};
        if (token.kind == TokenKind::Comment)
            code = line.left(token.start);
    }

    const QStringList words = splitWords(code, &environment);
    if (words.isEmpty())
        return {};

    if (words.first() == QLatin1String("cd") && words.size() <= 2)
        return {BuiltinKind::ChangeDirectory, words.value(1)};
    if ((words.first() == QLatin1String("clear") || words.first() == QLatin1String("cls")) && words.size() == 1)
        return {BuiltinKind::Clear, {}};
    return {};
}

CommandKind classifyCommand(const QString& word, const QString& workingDirectory)
{
    if (word.isEmpty())
        return CommandKind::Unknown;
    if (isShellBuiltin(word))
        return CommandKind::ConsoleBuiltin;

    if (word.contains(QLatin1Char('/')) || word.contains(QLatin1Char('\\')))
    {
        QString path = word;
        if (path.startsWith(QLatin1String("~/")))
            path = QDir::homePath() + path.mid(1);
        const QFileInfo info(QDir::isAbsolutePath(path) ? path : QDir(workingDirectory).filePath(path));
        return info.isFile() && info.isExecutable() ? CommandKind::Executable : CommandKind::Unknown;
    }

    return QStandardPaths::findExecutable(word).isEmpty() ? CommandKind::Unknown : CommandKind::Executable;
}

QString joinPastedLines(const QString& text)
{
    QString normalized = text;
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QString result;
    bool continued = false; // the previous line ended with a backslash
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines)
    {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        if (!result.isEmpty())
            result += continued ? QLatin1String(" ") : QLatin1String("; ");

        continued = kBackslashEscapes && line.endsWith(QLatin1Char('\\'));
        result += continued ? line.chopped(1).trimmed() : line;
    }
    return result;
}

QString formatDuration(qint64 milliseconds)
{
    if (milliseconds < 0)
        milliseconds = 0;
    if (milliseconds < 1000)
        return QStringLiteral("%1 ms").arg(milliseconds);
    if (milliseconds < 60'000)
    {
        const qint64 tenths = milliseconds / 100; // cut, not rounded: 59.96 s must not become "60.0 s"
        return QStringLiteral("%1.%2 s").arg(tenths / 10).arg(tenths % 10);
    }
    if (milliseconds < 3'600'000)
    {
        const qint64 seconds = milliseconds / 1000;
        return QStringLiteral("%1m %2s").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    const qint64 minutes = milliseconds / 60'000;
    return QStringLiteral("%1h %2m").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

bool isListingCommand(const QString& line)
{
    static const QSet<QString> programs = wordSet({"ls", "dir", "vdir", "find", "fd", "fdfind", "eza", "exa"});
    for (const Token& token : tokenize(line))
    {
        if (token.kind != TokenKind::Command)
            continue;
        QString name = QFileInfo(splitWords(line.mid(token.start, token.length)).value(0)).fileName().toLower();
        if (name.endsWith(QLatin1String(".exe")))
            name.chop(4);
        if (programs.contains(name))
            return true;
    }
    return false;
}

QString resolvePath(const QString& baseDirectory, const QString& path)
{
    QString resolved = path;
    if (resolved == QLatin1String("~"))
        resolved = QDir::homePath();
    else if (resolved.startsWith(QLatin1String("~/")) || resolved.startsWith(QLatin1String("~\\")))
        resolved = QDir::homePath() + resolved.mid(1);

    if (QDir::isAbsolutePath(resolved))
        return QDir::cleanPath(resolved);
    return QDir::cleanPath(QDir(baseDirectory).filePath(resolved));
}

QStringList pathCandidatesAt(const QString& line, qsizetype column)
{
    QStringList candidates;
    auto add = [&](const QString& candidate) {
        const QString trimmed = candidate.trimmed();
        if (!trimmed.isEmpty() && !candidates.contains(trimmed))
            candidates << trimmed;
    };

    const qsizetype size = line.size();
    if (size == 0 || column < 0)
        return candidates;

    auto isSeparator = [](QChar c) { return c.isSpace() || QStringView(u"\"'`()<>[]{}|,;=").contains(c); };

    column = qMin(column, size);
    if (column > 0 && (column >= size || isSeparator(line[column])))
        --column; // the cursor is just behind the word
    if (column >= size || isSeparator(line[column]))
        return candidates;

    qsizetype begin = column;
    while (begin > 0 && !isSeparator(line[begin - 1]))
        --begin;
    qsizetype end = column;
    while (end < size && !isSeparator(line[end]))
        ++end;

    QString word = line.mid(begin, end - begin);
    while (word.size() > 1 && QStringView(u",:;!?").contains(word.back()))
        word.chop(1);
    if (word.size() > 2 && word.endsWith(QLatin1Char('.')) && !word.endsWith(QLatin1String("..")))
        word.chop(1);

    // A plain word of a sentence is not a path, even when a directory of that name exists
    const bool looksLikePath = word.contains(QLatin1Char('/')) || word.contains(QLatin1Char('\\')) || word == QLatin1String("~");
    if (looksLikePath)
    {
        add(word);
        static const QRegularExpression lineAndColumn(QStringLiteral(R"((:\d+)+$)")); // file.cpp:12:5
        QString withoutPosition = word;
        withoutPosition.remove(lineAndColumn);
        add(withoutPosition);
    }

    if (const auto row = parseListingRow(line))
        add(line.mid(row->nameStart));
    else
        add(line); // a name in a listing (`ls` prints one name per line when it is not a terminal)

    return candidates;
}

std::optional<ListingClassifier::Range> ListingClassifier::directoryRange(const QString& rawLine)
{
    QString line = rawLine;
    if (line.endsWith(QLatin1Char('\r')))
        line.chop(1);
    if (line.trimmed().isEmpty())
        return std::nullopt;

    if (const auto row = parseListingRow(line))
    {
        if (!row->directory)
            return std::nullopt;
        return Range{row->nameStart, line.size() - row->nameStart};
    }

    const QString text = line.trimmed();
    const qsizetype start = line.indexOf(text);
    const QString contextDirectory = context_.isEmpty() ? baseDirectory_ : resolvePath(baseDirectory_, context_);

    // `ls -R` and `ls a b`: "path:" and then the names which are in it
    if (text.endsWith(QLatin1Char(':')) && text.size() > 1)
    {
        const QString header = text.chopped(1);
        if (QFileInfo(resolvePath(baseDirectory_, header)).isDir())
        {
            context_ = header;
            return std::nullopt;
        }
    }

    QString name = text;
    while (name.size() > 1 && name.endsWith(QLatin1Char('/'))) // `ls -F`, `find` with a trailing slash
        name.chop(1);
    if (QFileInfo(resolvePath(contextDirectory, name)).isDir())
        return Range{start, text.size()};
    return std::nullopt;
}
} // namespace ConsoleShell
