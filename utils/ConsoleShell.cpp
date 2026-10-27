#include "ConsoleShell.h"

#include <QChar>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QStringView>

namespace ConsoleShell
{
namespace
{
using namespace Qt::StringLiterals;

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
    static const QSet<QString> prefixes{u"if"_s, u"then"_s, u"else"_s, u"elif"_s, u"do"_s, u"while"_s, u"until"_s,
                                        u"!"_s,  u"time"_s, u"sudo"_s, u"exec"_s, u"nohup"_s, u"command"_s};
    return prefixes.contains(word.toString());
}

/// Words handled by the shell itself (they are not programs on the disk, or not always).
bool isShellBuiltin(const QString& word)
{
    static const QSet<QString> builtins{
        u"cd"_s,    u"clear"_s, u"cls"_s,   u"echo"_s,    u"export"_s, u"unset"_s,  u"source"_s, u"."_s,
        u"alias"_s, u"set"_s,   u"read"_s,  u"pwd"_s,     u"exit"_s,   u"exec"_s,   u"eval"_s,    u"type"_s,
        u"test"_s,  u"["_s,     u"if"_s,    u"then"_s,     u"else"_s,   u"elif"_s,   u"fi"_s,      u"for"_s,
        u"in"_s,    u"do"_s,    u"done"_s,  u"while"_s,    u"until"_s,  u"case"_s,   u"esac"_s,    u"time"_s,
        u"wait"_s,  u"ulimit"_s, u"umask"_s, u"history"_s, u"command"_s, u"dir"_s,   u"copy"_s,    u"del"_s,
        u"move"_s,  u"ren"_s,   u"mkdir"_s, u"rmdir"_s,   u"md"_s,     u"rd"_s,     u"chdir"_s};
    return builtins.contains(word);
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
} // namespace ConsoleShell
