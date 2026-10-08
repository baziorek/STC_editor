#include "StcTagRegions.h"

#include <algorithm>
#include <optional>
#include <QRegularExpression>

namespace
{
using StcTagRegions::Range;

constexpr int kMaxTagLength = 2000; // an opening bracket without the closing one must not make us read the whole document

struct Token
{
    bool closing = false;
    QString name; // lower case
    Range range;  // the whole `[...]`
    QList<Range> attributeValues; // between the quotes
};

bool isAsciiLetter(QChar c)
{
    return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'));
}

bool isVerbatimTag(const QString& name)
{
    return name == QLatin1String("cpp") || name == QLatin1String("code") || name == QLatin1String("py") || name == QLatin1String("log");
}

/// Reads `[name attr="value" ...]` or `[/name]` which starts with the `[` at `start`.
std::optional<Token> parseTagAt(const QString& text, int start)
{
    const int size = text.size();
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
        if (ch == QLatin1Char('"') || ch == QLatin1Char('\''))
        {
            if (token.closing)
                return std::nullopt;
            quote = ch;
            valueStart = p + 1;
        }
    }
    return std::nullopt;
}

struct Pair
{
    Range open;  // the tokens, with the brackets
    Range close;

    Range content() const { return {open.end, close.start}; }
};
} // namespace

QList<StcTagRegions::Range> StcTagRegions::regionsAround(const QString& text, int position)
{
    struct Opened
    {
        QString name;
        Range range;
    };

    QList<Token> openingTags;
    QList<Pair> pairs;
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
                    pairs.append({stack[k].range, token->range});
                    stack.erase(stack.begin() + k, stack.end());
                    break;
                }
            }
            continue;
        }

        openingTags.append(*token);

        if (isVerbatimTag(token->name))
        {
            const QRegularExpression closingTag(QStringLiteral("\\[/\\s*%1\\s*\\]").arg(token->name), QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = closingTag.match(text, token->range.end);
            if (match.hasMatch())
            {
                pairs.append({token->range, {static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedEnd())}});
                i = static_cast<int>(match.capturedEnd()); // nothing inside is a tag
            }
            continue;
        }

        stack.append({token->name, token->range});
    }

    QList<Range> regions;
    auto add = [&regions](const Range& range) {
        if (range.length() > 0 && !regions.contains(range))
            regions.append(range);
    };

    // 1. the attribute value in which the position is
    for (const Token& token : std::as_const(openingTags))
    {
        if (position < token.range.start || position > token.range.end)
            continue;
        for (const Range& value : token.attributeValues)
        {
            if (position >= value.start && position <= value.end)
            {
                add(value);
                break;
            }
        }
        break; // the tokens do not overlap
    }

    // 2. the content of the pairs of tags around the position (the position may be in a tag itself), the smallest first
    QList<Range> around;
    for (const Pair& pair : std::as_const(pairs))
    {
        if (position >= pair.open.start && position <= pair.close.end)
            around.append(pair.content());
    }
    std::stable_sort(around.begin(), around.end(), [](const Range& a, const Range& b) { return a.length() < b.length(); });
    for (const Range& range : std::as_const(around))
        add(range);

    return regions;
}
