#include "ConsoleSyntaxHighlighter.h"

#include <QColor>
#include <QFont>
#include <QRgb>

namespace
{
constexpr qint64 kCommandCacheLifetimeMs = 5000;

struct Palette
{
    QRgb found, builtin, unknown, assignment, option, string, variable, op, comment;
};

constexpr Palette kLight{0x2e7d32, 0x1565c0, 0xc62828, 0x8e24aa, 0x00838f, 0x9a6700, 0x8e24aa, 0xad1457, 0x757575};
constexpr Palette kDark{0x7fd962, 0x6cb6ff, 0xff6b68, 0xd78bff, 0x56d4dd, 0xe5c07b, 0xd78bff, 0xff9ac1, 0x8a8a8a};
} // namespace

ConsoleSyntaxHighlighter::ConsoleSyntaxHighlighter(QTextDocument* document) : QSyntaxHighlighter(document)
{
    cacheAge_.start();
    rebuildFormats();
}

void ConsoleSyntaxHighlighter::setWorkingDirectory(const QString& directory)
{
    if (workingDirectory_ == directory)
        return;
    workingDirectory_ = directory;
    commandCache_.clear(); // `./app` may exist here and not there
    rehighlight();
}

void ConsoleSyntaxHighlighter::setDarkBackground(bool dark)
{
    if (dark_ == dark)
        return;
    dark_ = dark;
    rebuildFormats();
    rehighlight();
}

void ConsoleSyntaxHighlighter::rebuildFormats()
{
    const Palette& palette = dark_ ? kDark : kLight;
    auto make = [](QRgb color, bool bold = false, bool italic = false) {
        QTextCharFormat format;
        format.setForeground(QColor(color));
        if (bold)
            format.setFontWeight(QFont::Bold);
        format.setFontItalic(italic);
        return format;
    };
    commandFound_ = make(palette.found, /*bold=*/true);
    commandBuiltin_ = make(palette.builtin, /*bold=*/true);
    commandUnknown_ = make(palette.unknown, /*bold=*/true);
    assignment_ = make(palette.assignment);
    option_ = make(palette.option);
    string_ = make(palette.string);
    variable_ = make(palette.variable);
    operator_ = make(palette.op, /*bold=*/true);
    comment_ = make(palette.comment, /*bold=*/false, /*italic=*/true);
}

ConsoleShell::CommandKind ConsoleSyntaxHighlighter::commandKind(const QString& word)
{
    if (cacheAge_.hasExpired(kCommandCacheLifetimeMs))
    {
        commandCache_.clear();
        cacheAge_.restart();
    }
    auto it = commandCache_.constFind(word);
    if (it == commandCache_.constEnd())
        it = commandCache_.insert(word, ConsoleShell::classifyCommand(word, workingDirectory_));
    return it.value();
}

QTextCharFormat ConsoleSyntaxHighlighter::formatFor(ConsoleShell::TokenKind kind) const
{
    using ConsoleShell::TokenKind;
    switch (kind)
    {
    case TokenKind::Assignment: return assignment_;
    case TokenKind::Option:     return option_;
    case TokenKind::String:     return string_;
    case TokenKind::Variable:   return variable_;
    case TokenKind::Operator:   return operator_;
    case TokenKind::Comment:    return comment_;
    case TokenKind::Command:    // chosen by the caller, it depends on the word
    case TokenKind::Argument:   break;
    }
    return {};
}

void ConsoleSyntaxHighlighter::highlightBlock(const QString& text)
{
    using ConsoleShell::CommandKind;
    using ConsoleShell::TokenKind;

    for (const ConsoleShell::Token& token : ConsoleShell::tokenize(text))
    {
        if (token.kind == TokenKind::Argument)
            continue;

        if (token.kind == TokenKind::Command)
        {
            // the word may be quoted ("my prog"): the shell looks for the program without the quotes
            const QString word = text.mid(token.start, token.length);
            switch (commandKind(ConsoleShell::splitWords(word).value(0)))
            {
            case CommandKind::ConsoleBuiltin: setFormat(token.start, token.length, commandBuiltin_); break;
            case CommandKind::Executable:     setFormat(token.start, token.length, commandFound_); break;
            case CommandKind::Unknown:        setFormat(token.start, token.length, commandUnknown_); break;
            }
            continue;
        }

        setFormat(token.start, token.length, formatFor(token.kind));
    }
}
