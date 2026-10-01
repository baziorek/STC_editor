#include "CodeLanguageHighlighting.h"

#include <QFile>
#include <QLanguage>    // from QCodeEditor
#include <QRegularExpression>
#include <QSet>
#include <QSyntaxStyle> // from QCodeEditor
#include <utility>

// Q_INIT_RESOURCE declares a function, so it has to be used outside of any namespace.
static void initQCodeEditorResources()
{
    Q_INIT_RESOURCE(qcodeeditor_resources);
}

namespace stc::codehl
{
namespace
{
bool isIdentifierStart(QChar c)
{
    return c.isLetter() || c == u'_';
}

bool isIdentifierPart(QChar c)
{
    return c.isLetterOrNumber() || c == u'_';
}

/// Applies the named format of QCodeEditor's default style to the range `[start, end)`.
class Painter
{
public:
    explicit Painter(const FormatSetter& setFormat)
        : setFormat(setFormat), style(QSyntaxStyle::defaultStyle())
    {}

    void operator()(int start, int end, const char* formatName) const
    {
        if (end > start)
            setFormat(start, end - start, style->getFormat(formatName));
    }

private:
    const FormatSetter& setFormat;
    QSyntaxStyle* style;
};

// ------------------------------------------------------------------ Python

struct PythonLexicon
{
    QSet<QString> keywords;
    QSet<QString> builtinFunctions;
    QSet<QString> builtinTypes;

    PythonLexicon()
    {
        // The base word lists come from QCodeEditor's python.xml...
        initQCodeEditorResources();
        QFile file(":/languages/python.xml");
        if (file.open(QIODevice::ReadOnly))
        {
            QLanguage language(&file);
            if (language.isLoaded())
            {
                const auto addNames = [&language](const QString& section, QSet<QString>& target) {
                    for (const auto& name : language.names(section))
                        target.insert(name);
                };
                addNames("Keyword", keywords);
                addNames("Function", builtinFunctions);
                addNames("PrimitiveType", builtinTypes);
            }
        }

        // ...but that file is tiny (and lists `do`, which is not a Python keyword), so it is completed here.
        for (const char* word : { "as", "assert", "async", "await", "class", "def", "del", "elif", "except",
                                  "finally", "from", "global", "lambda", "nonlocal", "pass", "raise", "try",
                                  "with", "yield" })
            keywords.insert(word);
        keywords.remove("do");

        keywords.remove("enumerate"); // it is a builtin function, not a keyword
        for (const char* word : { "enumerate", "print", "range", "sum", "abs", "sorted", "reversed", "zip", "map",
                                  "filter", "open", "input", "isinstance", "issubclass", "getattr", "setattr",
                                  "hasattr", "delattr", "super", "iter", "next", "repr", "format", "id", "hash",
                                  "any", "all", "round", "divmod", "pow", "ord", "chr", "bin", "hex", "oct",
                                  "callable", "vars", "dir", "globals", "locals", "eval", "exec" })
            builtinFunctions.insert(word);

        for (const char* word : { "None", "bytes", "bytearray", "list", "tuple", "frozenset", "complex", "object",
                                  "type" })
            builtinTypes.insert(word);
    }
};

const PythonLexicon& pythonLexicon()
{
    static const PythonLexicon lexicon;
    return lexicon;
}

/// Index of the first quote of a closing `"""` / `'''` searching in `[start, to)`, -1 when there is none.
int findTripleQuoteEnd(const QString& text, QChar quote, int start, int to)
{
    for (int i = start; i < to;)
    {
        if (text[i] == u'\\')
        {
            i += 2; // skip the escaped character
            continue;
        }
        if (text[i] == quote && i + 2 < to && text[i + 1] == quote && text[i + 2] == quote)
            return i;
        ++i;
    }
    return -1;
}

bool isStringPrefixLetter(QChar c)
{
    switch (c.toLower().unicode())
    {
    case u'r': case u'u': case u'b': case u'f':
        return true;
    default:
        return false;
    }
}

/// Index of the opening quote if a string literal (with an optional prefix: r, u, b, f, rb, fr, ...)
/// starts at `pos`, otherwise -1.
int stringQuoteIndex(const QString& text, int pos, int to)
{
    static const QSet<QString> validPrefixes = { "", "r", "u", "b", "f", "br", "rb", "fr", "rf" };

    int i = pos;
    while (i < to && i - pos < 2 && isStringPrefixLetter(text[i]))
        ++i;

    if (i < to && (text[i] == u'"' || text[i] == u'\'') && validPrefixes.contains(text.mid(pos, i - pos).toLower()))
        return i;
    return -1;
}
} // namespace

int highlightPython(const QString& text, int from, int to, int stateIn, const FormatSetter& setFormat)
{
    from = std::max(from, 0);
    to = std::min<int>(to, text.size());
    int state = stateIn & PY_STATE_MASK;
    if (from >= to)
        return state; // e.g. an empty line inside of a triple-quoted string

    static const QRegularExpression numberRe(
        R"(\G(?:0[xX][0-9a-fA-F_]+|0[oO][0-7_]+|0[bB][01_]+|(?:\d[\d_]*\.?[\d_]*|\.\d[\d_]*)(?:[eE][+-]?\d[\d_]*)?[jJ]?))");

    const Painter paint(setFormat);
    const auto& lexicon = pythonLexicon();

    int i = from;

    // a triple-quoted string which began on one of the previous lines
    if (state != PY_STATE_NONE)
    {
        const QChar quote = (state == PY_STATE_TRIPLE_DOUBLE) ? u'"' : u'\'';
        const int end = findTripleQuoteEnd(text, quote, i, to);
        if (end < 0)
        {
            paint(i, to, "String");
            return state;
        }
        paint(i, end + 3, "String");
        i = end + 3;
        state = PY_STATE_NONE;
    }

    enum class Expect { Nothing, FunctionName, ClassName } expect = Expect::Nothing;
    bool seenToken = false; // used to tell a decorator (`@name` at the line start) from the `@` operator

    while (i < to)
    {
        const QChar c = text[i];
        if (c.isSpace())
        {
            ++i;
            continue;
        }

        if (c == u'#')
        {
            paint(i, to, "Comment");
            return PY_STATE_NONE;
        }

        const bool previouslySeenToken = std::exchange(seenToken, true);
        const Expect expected = std::exchange(expect, Expect::Nothing);

        // string literal
        const int quoteIndex = (c == u'"' || c == u'\'' || isIdentifierStart(c)) ? stringQuoteIndex(text, i, to) : -1;
        if (quoteIndex >= 0)
        {
            const QChar quote = text[quoteIndex];
            const bool triple = quoteIndex + 2 < to && text[quoteIndex + 1] == quote && text[quoteIndex + 2] == quote;
            if (triple)
            {
                const int end = findTripleQuoteEnd(text, quote, quoteIndex + 3, to);
                if (end < 0)
                {
                    paint(i, to, "String");
                    return quote == u'"' ? PY_STATE_TRIPLE_DOUBLE : PY_STATE_TRIPLE_SINGLE;
                }
                paint(i, end + 3, "String");
                i = end + 3;
            }
            else
            {
                int j = quoteIndex + 1;
                while (j < to && text[j] != quote)
                    j += (text[j] == u'\\') ? 2 : 1;
                j = std::min(j + 1, to); // including the closing quote, if the string is closed
                paint(i, j, "String");
                i = j;
            }
            continue;
        }

        // number
        if (c.isDigit() || (c == u'.' && i + 1 < to && text[i + 1].isDigit()))
        {
            const auto match = numberRe.match(text, i);
            const int length = match.hasMatch() ? std::min<int>(match.capturedLength(), to - i) : 1;
            paint(i, i + length, "Number");
            i += length;
            continue;
        }

        // decorator
        if (c == u'@' && !previouslySeenToken)
        {
            int j = i + 1;
            while (j < to && (isIdentifierPart(text[j]) || text[j] == u'.'))
                ++j;
            paint(i, j, "Preprocessor");
            i = j;
            continue;
        }

        // identifier, keyword, builtin
        if (isIdentifierStart(c))
        {
            int j = i + 1;
            while (j < to && isIdentifierPart(text[j]))
                ++j;
            const QString word = text.mid(i, j - i);

            int before = i - 1;
            while (before >= from && text[before].isSpace())
                --before;
            const bool isAttribute = before >= from && text[before] == u'.'; // `obj.print` is not a keyword

            int after = j;
            while (after < to && text[after].isSpace())
                ++after;
            const bool isCall = after < to && text[after] == u'(';

            const char* formatName = nullptr;
            if (expected == Expect::FunctionName)
                formatName = "Function";
            else if (expected == Expect::ClassName)
                formatName = "Type";
            else if (!isAttribute && lexicon.keywords.contains(word))
                formatName = "Keyword";
            else if (!isAttribute && lexicon.builtinTypes.contains(word))
                formatName = "PrimitiveType";
            else if (!isAttribute && lexicon.builtinFunctions.contains(word))
                formatName = "Function";
            else if (isCall)
                formatName = "Function";

            if (formatName)
                paint(i, j, formatName);

            if (expected == Expect::Nothing && !isAttribute)
            {
                if (word == QLatin1String("def"))
                    expect = Expect::FunctionName;
                else if (word == QLatin1String("class"))
                    expect = Expect::ClassName;
            }
            i = j;
            continue;
        }

        ++i; // an operator or punctuation - left with the default format
    }
    return state;
}

// ------------------------------------------------------------------ XML

int highlightXml(const QString& text, int from, int to, int stateIn, const FormatSetter& setFormat)
{
    from = std::max(from, 0);
    to = std::min<int>(to, text.size());
    int state = stateIn & XML_STATE_MASK;
    if (from >= to)
        return state;

    const Painter paint(setFormat);

    int i = from;
    while (i < to)
    {
        if (state & XML_STATE_COMMENT)
        {
            const int end = text.indexOf(QLatin1String("-->"), i);
            if (end < 0 || end + 3 > to)
            {
                paint(i, to, "Comment");
                return state;
            }
            paint(i, end + 3, "Comment");
            i = end + 3;
            state &= ~XML_STATE_COMMENT;
            continue;
        }

        if (state & XML_STATE_IN_TAG)
        {
            const QChar c = text[i];
            if (c.isSpace())
            {
                ++i;
            }
            else if (c == u'>' || ((c == u'/' || c == u'?') && i + 1 < to && text[i + 1] == u'>'))
            {
                const int length = (c == u'>') ? 1 : 2;
                paint(i, i + length, "Keyword");
                i += length;
                state &= ~XML_STATE_IN_TAG;
            }
            else if (c == u'"' || c == u'\'')
            {
                int j = i + 1;
                while (j < to && text[j] != c)
                    ++j;
                j = std::min(j + 1, to);
                paint(i, j, "String");
                i = j;
            }
            else if (isIdentifierStart(c))
            {
                int j = i + 1;
                while (j < to && (isIdentifierPart(text[j]) || text[j] == u':' || text[j] == u'-' || text[j] == u'.'))
                    ++j;
                paint(i, j, "Function"); // attribute name
                i = j;
            }
            else
            {
                ++i; // `=` and whatever else
            }
            continue;
        }

        // plain text, up to the next tag
        const int open = text.indexOf(u'<', i);
        if (open < 0 || open >= to)
            return state;

        if (open + 4 <= to && text.mid(open, 4) == QLatin1String("<!--"))
        {
            state |= XML_STATE_COMMENT;
            paint(open, open + 4, "Comment");
            i = open + 4;
            continue;
        }

        // `<`, `</`, `<?` or `<!` followed by the name of the element
        int j = open + 1;
        if (j < to && (text[j] == u'/' || text[j] == u'?' || text[j] == u'!'))
            ++j;
        paint(open, j, "Keyword");
        const int nameStart = j;
        while (j < to && (isIdentifierPart(text[j]) || text[j] == u':' || text[j] == u'-' || text[j] == u'.'))
            ++j;
        paint(nameStart, j, "Keyword");
        state |= XML_STATE_IN_TAG;
        i = j;
    }
    return state;
}

// ------------------------------------------------------------------ JSON

void highlightJson(const QString& text, int from, int to, const FormatSetter& setFormat)
{
    from = std::max(from, 0);
    to = std::min<int>(to, text.size());

    static const QRegularExpression numberRe(R"(\G-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)");

    const Painter paint(setFormat);

    int i = from;
    while (i < to)
    {
        const QChar c = text[i];

        if (c == u'"')
        {
            int j = i + 1;
            while (j < to && text[j] != u'"')
                j += (text[j] == u'\\') ? 2 : 1;
            j = std::min(j + 1, to);

            int next = j;
            while (next < to && text[next].isSpace())
                ++next;
            const bool isKey = next < to && text[next] == u':';

            paint(i, j, isKey ? "Type" : "String");
            i = j;
        }
        else if (c.isDigit() || (c == u'-' && i + 1 < to && text[i + 1].isDigit()))
        {
            const auto match = numberRe.match(text, i);
            const int length = match.hasMatch() ? std::min<int>(match.capturedLength(), to - i) : 1;
            paint(i, i + length, "Number");
            i += length;
        }
        else if (c.isLetter())
        {
            int j = i + 1;
            while (j < to && text[j].isLetter())
                ++j;
            const QString word = text.mid(i, j - i);
            if (word == QLatin1String("true") || word == QLatin1String("false") || word == QLatin1String("null"))
                paint(i, j, "Keyword");
            i = j;
        }
        else
        {
            ++i;
        }
    }
}
} // namespace stc::codehl
