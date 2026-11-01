#include "FoldRegions.h"

#include <QHash>
#include <QRegularExpression>
#include <algorithm>

namespace stcfold
{
namespace
{
// The same tags and the same form of the opening tag as CodeEditor::parseAllCodeBlocks() recognizes
const QRegularExpression& openingTokenRe()
{
    static const QRegularExpression re(
        R"__(\[(cpp|code|py|log)(\s+src\s*=\s*"[^"]*")?\]|\[h([1-6])(?:\s[^\]]*)?\])__",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression& closingTagRe(const QString& tag)
{
    static const QHash<QString, QRegularExpression> byTag = [] {
        QHash<QString, QRegularExpression> result;
        for (const QString& name : {QStringLiteral("cpp"), QStringLiteral("code"), QStringLiteral("py"), QStringLiteral("log")})
        {
            result.insert(name, QRegularExpression(QStringLiteral(R"(\[/\s*%1\s*\])").arg(name),
                                                   QRegularExpression::CaseInsensitiveOption));
        }
        return result;
    }();
    // `tag` is always one of the four names the opening regular expression accepts. (Not `byTag[tag]`: on a const
    // QHash it returns the regular expression by value, and the reference to that temporary would dangle.)
    return byTag.constFind(tag).value();
}

bool isEscaped(const QString& line, int tagStart)
{
    return tagStart > 0 && line[tagStart - 1] == QLatin1Char('\\');
}

bool isBlank(const QString& line)
{
    return line.trimmed().isEmpty();
}

struct HeaderMark
{
    int line;
    int level;
};
} // namespace

QVector<Region> findRegions(const QStringList& lines)
{
    QVector<HeaderMark> headers;
    QVector<Region> regions;

    QString openedTag;     // not empty: we are inside of a block of code, which waits for its closing tag
    int openedLine = -1;

    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        const QString& line = lines[lineNumber];
        int position = 0;
        bool headerInThisLine = false;

        while (position <= line.size())
        {
            if (!openedTag.isEmpty())
            {
                const auto closing = closingTagRe(openedTag).match(line, position);
                if (!closing.hasMatch())
                {
                    break; // the block goes on in the next line
                }

                // Text before the closing tag in its line is a part of the code, so that line cannot be hidden
                // while its tag stays visible. A line with only the closing tag stays visible.
                const bool closingLineHasCode = !isBlank(line.left(closing.capturedStart()));
                const int lastHiddenLine = closingLineHasCode ? lineNumber : lineNumber - 1;
                if (lastHiddenLine > openedLine)
                {
                    regions.push_back({openedLine, lastHiddenLine, Kind::Code, 0, openedTag, lineNumber});
                }
                openedTag.clear();
                position = closing.capturedEnd();
                continue;
            }

            const auto opening = openingTokenRe().match(line, position);
            if (!opening.hasMatch())
            {
                break;
            }
            position = opening.capturedEnd();
            if (isEscaped(line, opening.capturedStart()))
            {
                continue; // `\[cpp]` is a text, not a tag
            }

            if (opening.capturedStart(3) >= 0) // a header
            {
                if (!headerInThisLine)
                {
                    headers.push_back({lineNumber, opening.captured(3).toInt()});
                    headerInThisLine = true;
                }
            }
            else
            {
                openedTag = opening.captured(1).toLower();
                openedLine = lineNumber;
            }
        }
    }

    for (int index = 0; index < headers.size(); ++index)
    {
        const HeaderMark& header = headers[index];

        int lastLine = static_cast<int>(lines.size()) - 1;
        for (int next = index + 1; next < headers.size(); ++next)
        {
            if (headers[next].level <= header.level)
            {
                lastLine = headers[next].line - 1;
                break;
            }
        }
        // the empty lines before the next section stay visible: they separate the sections
        while (lastLine > header.line && isBlank(lines[lastLine]))
        {
            --lastLine;
        }
        if (lastLine > header.line)
        {
            regions.push_back({header.line, lastLine, Kind::Header, header.level, QStringLiteral("h%1").arg(header.level), lastLine});
        }
    }

    std::stable_sort(regions.begin(), regions.end(), [](const Region& a, const Region& b) {
        if (a.startLine != b.startLine)
            return a.startLine < b.startLine;
        return a.kind == Kind::Header && b.kind != Kind::Header; // a header first when both start in one line
    });
    return regions;
}
} // namespace stcfold
