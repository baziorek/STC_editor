#include "HtmlPrettyPrinter.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <vector>

namespace HtmlPrettyPrinter
{
namespace
{
constexpr int kIndentWidth = 2;
constexpr int kMaxIndentLevels = 40; ///< a deeper nesting is shown as deep as this, so a broken page does not make lines of spaces

const QSet<QString>& blockTags()
{
    static const QSet<QString> tags = {"html", "head", "body", "div", "p", "table", "thead", "tbody", "tfoot", "tr", "td", "th",
                                       "caption", "ul", "ol", "li", "dl", "dt", "dd", "h1", "h2", "h3", "h4", "h5", "h6",
                                       "blockquote", "section", "article", "header", "footer", "nav", "aside", "form",
                                       "fieldset", "pre", "hr", "title", "meta", "link"};
    return tags;
}

const QSet<QString>& voidTags()
{
    static const QSet<QString> tags = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"};
    return tags;
}

/// Elements whose content is not HTML (or is not to be changed): it is copied as it is
const QSet<QString>& verbatimTags()
{
    static const QSet<QString> tags = {"pre", "script", "style", "textarea"};
    return tags;
}

struct Token
{
    enum class Kind
    {
        Text,
        OpenTag,
        CloseTag,
        SingleTag, ///< a void element (`<br>`, `<img ...>`, `<x/>`)
        Comment,   ///< also `<!DOCTYPE ...>` and `<?...?>`
        Verbatim,  ///< a whole `<pre>...</pre>` (the tags and what is between them)
    };

    Kind kind = Kind::Text;
    QString text;
    QString name; ///< lower case name of the element (tags only)
};

QString tagName(const QString& tag)
{
    int i = 1;
    if (i < tag.size() && tag[i] == '/')
        ++i;

    const int start = i;
    while (i < tag.size() && (tag[i].isLetterOrNumber() || tag[i] == '-' || tag[i] == ':'))
        ++i;
    return tag.mid(start, i - start).toLower();
}

/// The end (after '>') of the tag which starts at `start`; a '>' in the quotes of an attribute does not end it. -1 when it is not closed.
int endOfTag(const QString& html, int start)
{
    QChar quote;
    for (int i = start + 1; i < html.size(); ++i)
    {
        const QChar c = html[i];
        if (!quote.isNull())
        {
            if (c == quote)
                quote = QChar();
        }
        else if (c == '"' || c == '\'')
            quote = c;
        else if (c == '>')
            return i + 1;
    }
    return -1;
}

/// Is `<` at `position` a start of a tag (`<a`, `</a`, `<!`, `<?`), and not just a character of a text (`a < b`)?
bool startsTag(const QString& html, int position)
{
    if (position + 1 >= html.size())
        return false;

    const QChar next = html[position + 1];
    return next.isLetter() || next == '/' || next == '!' || next == '?';
}

std::vector<Token> tokenize(const QString& html)
{
    std::vector<Token> tokens;

    int i = 0;
    while (i < html.size())
    {
        if (html[i] != '<' || !startsTag(html, i))
        {
            int end = i + 1;
            while (end < html.size() && !(html[end] == '<' && startsTag(html, end)))
                ++end;
            tokens.push_back({Token::Kind::Text, html.mid(i, end - i), {}});
            i = end;
            continue;
        }

        if (html.mid(i, 4) == QLatin1String("<!--"))
        {
            const int close = html.indexOf(QLatin1String("-->"), i + 4);
            const int end = close < 0 ? static_cast<int>(html.size()) : close + 3;
            tokens.push_back({Token::Kind::Comment, html.mid(i, end - i), {}});
            i = end;
            continue;
        }

        int end = endOfTag(html, i);
        if (end < 0)
        {
            // not closed: the rest is a text, so nothing is lost
            tokens.push_back({Token::Kind::Text, html.mid(i), {}});
            break;
        }

        const QString tag = html.mid(i, end - i);
        const QString name = tagName(tag);

        if (tag.startsWith(QLatin1String("<!")) || tag.startsWith(QLatin1String("<?")))
        {
            tokens.push_back({Token::Kind::Comment, tag, {}});
        }
        else if (tag.startsWith(QLatin1String("</")))
        {
            tokens.push_back({Token::Kind::CloseTag, tag, name});
        }
        else if (voidTags().contains(name) || tag.endsWith(QLatin1String("/>")))
        {
            tokens.push_back({Token::Kind::SingleTag, tag, name});
        }
        else if (verbatimTags().contains(name))
        {
            // up to the closing tag of the same element, the tags in between are not tags
            const QRegularExpression closing(QStringLiteral("</%1\\s*>").arg(QRegularExpression::escape(name)),
                                             QRegularExpression::CaseInsensitiveOption);
            const auto match = closing.match(html, end);
            const int blockEnd = match.hasMatch() ? static_cast<int>(match.capturedEnd()) : static_cast<int>(html.size());
            tokens.push_back({Token::Kind::Verbatim, html.mid(i, blockEnd - i), name});
            end = blockEnd;
        }
        else
        {
            tokens.push_back({Token::Kind::OpenTag, tag, name});
        }
        i = end;
    }
    return tokens;
}

class Writer
{
public:
    QString result() &&
    {
        startLine();
        return std::move(out);
    }

    void startLine()
    {
        if (!atLineStart)
        {
            while (out.endsWith(' '))
                out.chop(1);
            out += '\n';
            atLineStart = true;
        }
    }

    /// Adds to the current line (which is indented first when it is a new one)
    void inlineText(const QString& text)
    {
        if (atLineStart)
        {
            out += QString(std::min(depth, kMaxIndentLevels) * kIndentWidth, ' ');
            atLineStart = false;
        }
        out += text;
    }

    void ownLine(const QString& text)
    {
        startLine();
        inlineText(text);
        startLine();
    }

    void deeper() { ++depth; }
    void shallower() { depth = std::max(0, depth - 1); }

    bool isAtLineStart() const { return atLineStart; }

private:
    QString out;
    int depth = 0;
    bool atLineStart = true;
};

/// Runs of whitespace are one space, so it is shown the way a browser shows it
QString collapseWhitespace(const QString& text)
{
    QString result;
    result.reserve(text.size());

    bool previousWasSpace = false;
    for (const QChar c : text)
    {
        // the non-breaking space is not a whitespace in HTML, so it stays
        const bool space = c.isSpace() && c != QChar(0x00A0);
        if (space)
        {
            if (!previousWasSpace)
                result += ' ';
        }
        else
            result += c;
        previousWasSpace = space;
    }
    return result;
}
} // namespace


QString prettyPrint(const QString& html)
{
    Writer writer;

    for (const Token& token : tokenize(html))
    {
        const bool isBlock = blockTags().contains(token.name);

        switch (token.kind)
        {
        case Token::Kind::Text:
        {
            QString text = collapseWhitespace(token.text);
            if (writer.isAtLineStart() && text.startsWith(' '))
                text.remove(0, 1); // the line is indented already
            if (!text.isEmpty())
                writer.inlineText(text);
            break;
        }
        case Token::Kind::OpenTag:
            if (isBlock)
            {
                writer.ownLine(token.text);
                writer.deeper();
            }
            else
                writer.inlineText(token.text);
            break;
        case Token::Kind::CloseTag:
            if (isBlock)
            {
                writer.shallower();
                writer.ownLine(token.text);
            }
            else
                writer.inlineText(token.text);
            break;
        case Token::Kind::SingleTag:
            if (token.name == "br")
            {
                writer.inlineText(token.text);
                writer.startLine();
            }
            else if (isBlock)
                writer.ownLine(token.text);
            else
                writer.inlineText(token.text);
            break;
        case Token::Kind::Comment:
            writer.ownLine(token.text);
            break;
        case Token::Kind::Verbatim:
            writer.ownLine(token.text);
            break;
        }
    }
    return std::move(writer).result();
}
} // namespace HtmlPrettyPrinter
