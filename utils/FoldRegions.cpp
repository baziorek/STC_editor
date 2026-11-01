#include "FoldRegions.h"

#include "StcTagScanner.h"

#include <algorithm>

namespace stcfold
{
namespace
{
bool isEscaped(const QString& line, int tagStart)
{
    return tagStart > 0 && line[tagStart - 1] == QLatin1Char('\\');
}

bool isBlank(const QString& line)
{
    return line.trimmed().isEmpty();
}

/// 1..6 for `h1`..`h6`, 0 for any other name
int headerLevel(const QString& name)
{
    if (name.size() == 2 && name[0] == QLatin1Char('h') && name[1] >= QLatin1Char('1') && name[1] <= QLatin1Char('6'))
        return name[1].digitValue();
    return 0;
}

struct HeaderMark
{
    int line;
    int level;
};

struct OpenedBlock
{
    QString tag;
    int line;
};
} // namespace

QVector<Region> findRegions(const QStringList& lines)
{
    QVector<HeaderMark> headers;
    QVector<Region> regions;

    QString verbatimTag;       // not empty: we are inside of a block of code, which waits for its closing tag
    int verbatimLine = -1;
    QVector<OpenedBlock> openedDivs;

    // The block is folded by hiding the lines after the one with the opening tag. The line with the closing tag stays,
    // unless there is code before the tag in it: then it is hidden with the block.
    auto addBlock = [&regions](int openLine, int closeLine, const QString& lineOfClosingTag, int closingTagStart, Kind kind, const QString& tag) {
        const bool closingLineHasContent = !isBlank(lineOfClosingTag.left(closingTagStart));
        const int lastHiddenLine = closingLineHasContent ? closeLine : closeLine - 1;
        if (lastHiddenLine > openLine)
        {
            regions.push_back({openLine, lastHiddenLine, kind, 0, tag, closeLine});
        }
    };

    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        const QString& line = lines[lineNumber];
        bool headerInThisLine = false;

        int position = 0;
        while ((position = static_cast<int>(line.indexOf(QLatin1Char('['), position))) >= 0)
        {
            const std::optional<StcTagScanner::Token> token = StcTagScanner::parseTagAt(line, position);
            if (!token)
            {
                ++position;
                continue;
            }
            const int tagStart = position;
            position = token->range.end;

            if (!verbatimTag.isEmpty())
            {
                // the content is verbatim: only the closing tag of the block means something
                if (token->closing && token->name == verbatimTag)
                {
                    addBlock(verbatimLine, lineNumber, line, tagStart, Kind::Code, verbatimTag);
                    verbatimTag.clear();
                }
                continue;
            }

            if (token->closing)
            {
                if (token->name == QLatin1String("div") && !openedDivs.isEmpty())
                {
                    addBlock(openedDivs.takeLast().line, lineNumber, line, tagStart, Kind::Div, token->name);
                }
                continue;
            }
            if (isEscaped(line, tagStart))
            {
                continue; // `\[cpp]` is a text, not a tag
            }

            if (const int level = headerLevel(token->name); level > 0)
            {
                if (!headerInThisLine)
                {
                    headers.push_back({lineNumber, level});
                    headerInThisLine = true;
                }
            }
            else if (StcTagScanner::isVerbatimTag(token->name))
            {
                verbatimTag = token->name;
                verbatimLine = lineNumber;
            }
            else if (token->name == QLatin1String("div"))
            {
                openedDivs.push_back({token->name, lineNumber});
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
        if ((a.kind == Kind::Header) != (b.kind == Kind::Header))
            return a.kind == Kind::Header; // a header first when regions start in one line
        return a.extentEndLine > b.extentEndLine; // then the bigger (outer) one
    });
    return regions;
}
} // namespace stcfold
