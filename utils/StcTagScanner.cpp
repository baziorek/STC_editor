#include "StcTagScanner.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace StcTagScanner
{
namespace
{
constexpr int kMaxTagLength = 2000; // an opening bracket without the closing one must not make us read the whole document

const QStringList& verbatimTagNames()
{
    static const QStringList names = {QStringLiteral("cpp"), QStringLiteral("code"), QStringLiteral("py"), QStringLiteral("log")};
    return names;
}

bool isAsciiLetter(QChar c)
{
    return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
}
} // namespace

bool isVerbatimTag(const QString& name)
{
    return verbatimTagNames().contains(name);
}

QString verbatimTagsAlternation()
{
    return verbatimTagNames().join(QLatin1Char('|'));
}

std::optional<Token> parseTagAt(const QString& text, int start)
{
    const int size = static_cast<int>(text.size());
    int p = start + 1;

    Token token;
    if (p < size && text[p] == QLatin1Char('/'))
    {
        token.closing = true;
        ++p;
        while (p < size && (text[p] == QLatin1Char(' ') || text[p] == QLatin1Char('\t')))
            ++p;
    }

    if (p >= size || !isAsciiLetter(text[p]))
        return std::nullopt; // `[0]`, `[=]`, `[]`...

    const int nameStart = p;
    while (p < size && (isAsciiLetter(text[p]) || text[p].isDigit() || text[p] == QLatin1Char('_')))
        ++p;
    token.name = text.mid(nameStart, p - nameStart).toLower();
    token.nameRange = {nameStart, p};

    // after the name: the end of the tag, a space or `=` (`[color=red]`); `[i++]` is not a tag
    if (p >= size || !(text[p] == QLatin1Char(']') || text[p] == QLatin1Char(' ') || text[p] == QLatin1Char('\t') || text[p] == QLatin1Char('=')))
        return std::nullopt;

    QChar quote;
    int valueStart = 0;
    for (; p < size && p - start < kMaxTagLength; ++p)
    {
        const QChar ch = text[p];
        if (ch == QLatin1Char('\n'))
            return std::nullopt; // tags do not span lines

        if (!quote.isNull())
        {
            if (ch == quote)
            {
                token.attributeValues.append({valueStart, p});
                quote = QChar();
            }
            continue;
        }

        if (ch == QLatin1Char(']'))
        {
            token.range = {start, p + 1};
            return token;
        }
        if (ch == QLatin1Char('['))
            return std::nullopt; // the `[` we started from was not a tag
        if (ch == QLatin1Char('\"') || ch == QLatin1Char('\''))
        {
            if (token.closing)
                return std::nullopt;
            quote = ch;
            valueStart = p + 1;
        }
    }
    return std::nullopt;
}

bool containsVerbatimTag(const QString& text)
{
    int i = 0;
    while ((i = static_cast<int>(text.indexOf(QLatin1Char('['), i))) >= 0)
    {
        const std::optional<Token> token = parseTagAt(text, i);
        if (token && isVerbatimTag(token->name))
            return true;
        i = token ? token->range.end : i + 1;
    }
    return false;
}

Scan scan(const QString& text)
{
    struct Opened
    {
        QString name;
        Range range;
        Range nameRange;
    };

    Scan result;
    QList<Opened> stack;

    int i = 0;
    while ((i = static_cast<int>(text.indexOf(QLatin1Char('['), i))) >= 0)
    {
        const std::optional<Token> token = parseTagAt(text, i);
        if (!token)
        {
            ++i;
            continue;
        }
        i = token->range.end;

        if (token->closing)
        {
            // pairs with the nearest opening tag of this name; the tags opened in between had no closing tag
            for (int k = static_cast<int>(stack.size()) - 1; k >= 0; --k)
            {
                if (stack[k].name == token->name)
                {
                    result.pairs.append({stack[k].range, token->range, stack[k].nameRange, token->nameRange});
                    stack.erase(stack.begin() + k, stack.end());
                    break;
                }
            }
            continue;
        }

        result.openingTags.append(*token);

        if (isVerbatimTag(token->name))
        {
            const QRegularExpression closingTag(QStringLiteral("\\[/\\s*%1\\s*\\]").arg(token->name), QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = closingTag.match(text, token->range.end);
            if (match.hasMatch())
            {
                const Range closing{static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedEnd())};
                int nameStart = closing.start + 2;
                while (nameStart < closing.end && text[nameStart].isSpace())
                    ++nameStart;
                result.pairs.append({token->range, closing, token->nameRange, {nameStart, nameStart + static_cast<int>(token->name.size())}});
                i = closing.end; // nothing inside is a tag
            }
            continue;
        }

        stack.append({token->name, token->range, token->nameRange});
    }
    return result;
}

std::optional<Range> matchingTag(const QString& text, Range selection)
{
    if (selection.length() <= 0)
        return std::nullopt;

    const Scan found = scan(text);
    for (const Pair& pair : found.pairs)
    {
        if (selection == pair.openName)
            return pair.closeName;
        if (selection == pair.closeName)
            return pair.openName;
        if (selection == pair.open)
            return pair.close;
        if (selection == pair.close)
            return pair.open;
    }
    return std::nullopt;
}
} // namespace StcTagScanner
