#include <QDebug>
#include <QRegularExpression>
#include "STCSyntaxHighlighter.h"
#include "../stcSyntaxPatterns.h"
#include "../types/stcTags.h"

namespace
{
enum BlockState // TODO: Why not to use `enum class StcTags: std::uint32_t` instead this:
{
    STATE_NONE            = -1,

    STATE_H1              = 0x01,
    STATE_H2              = 0x02,
    STATE_H3              = 0x04,

    STATE_DIV             = 0x08,
    DIV_CLASS_TIP         = 0x10,
    DIV_CLASS_UWAGA       = 0x20,
    DIV_CLASS_PLAIN       = 0x40,
    DIV_CLASS_CYTAT       = 0x80,

    STATE_CSV             = 0x100,
    STATE_PKT             = 0x200,

    STATE_CPP             = 0x400,
    STATE_CODE            = 0x800,
    STATE_CODE_CPP        = 0x1000,

    STATE_STYLE_BOLD      = 0x2000,
    STATE_STYLE_ITALIC    = 0x4000,
    STATE_STYLE_UNDERLINE = 0x8000,
    STATE_STYLE_STRIKE    = 0x10000,
};

constexpr bool PRINT_DEBUG = false; // TODO: Remove when formatting fully works
#define DEBUG(condition, text) if (PRINT_DEBUG && condition) qDebug() << "\t " << #text << " changes" << __LINE__ << currentBlockState()
#ifndef _WIN32
#warning "Chat helped me with the code, but it requires refactoring and corrections"
#endif
} // namespace


STCSyntaxHighlighter::STCSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    using enum StcTags;

    // Styl tagów (np. [code], [/div] itd.)
    QTextCharFormat tagFormat;
    tagFormat.setForeground(Qt::gray);
    tagFormat.setFontPointSize(8);
    rules.append({ stc::syntax::stdDefaultTagRe, tagFormat });

    // Nagłówki
    QColor headerColor{"#a33"};
    addBlockStyle(tagsClasses[H1], headerColor, std::to_underlying(BOLD), 20);
    addBlockStyle(tagsClasses[H2], headerColor, std::to_underlying(BOLD), 17);
    addBlockStyle(tagsClasses[H3], headerColor, std::to_underlying(NONE), 14);
    addBlockStyle(tagsClasses[H4], headerColor, std::to_underlying(NONE), 11);

    // DIV-y
    addBlockStyle("tip", Qt::white, std::to_underlying(NONE), -1, QColor("darkgreen"));
    addBlockStyle(tagsClasses[DIV], QColor::Invalid, std::to_underlying(NONE), -1, QColor("brown"));
    addBlockStyle("warning", Qt::white, std::to_underlying(NONE), -1, QColor("red"));
    addBlockStyle(tagsClasses[QUOTE], Qt::black, std::to_underlying(StcTags::NONE), -1, QColor("orange"));

    // Bloki kodu
    addBlockStyle(tagsClasses[CODE], QColor("yellow"), std::to_underlying(NONE), -1, QColor("black"), "monospace");
    addBlockStyle(tagsClasses[CPP], QColor("black"), std::to_underlying(NONE), -1, QColor("lightblue"), "monospace");
    addBlockStyle(tagsClasses[PY], QColor("black"), std::to_underlying(NONE), -1, QColor("brown"), "monospace");

    // Stylizacja tekstu (b/i/u/s)
    addBlockStyle(tagsClasses[BOLD], QColor::Invalid, std::to_underlying(BOLD));
    addBlockStyle(tagsClasses[ITALIC], QColor::Invalid, std::to_underlying(ITALIC));
    addBlockStyle(tagsClasses[UNDERLINED], QColor::Invalid, std::to_underlying(UNDERLINED));
    addBlockStyle(tagsClasses[STRUCK_OUT], QColor::Invalid, std::to_underlying(STRUCK_OUT));

    // CSV / PKT
    addBlockStyle("pkt", QColor("#000"), std::to_underlying(NONE), -1, QColor("#d0f0af"));
    addBlockStyle("csv", QColor("#000"), std::to_underlying(NONE), -1, QColor("#fce4b4"));

    // // Linki (a href=...)
    QTextCharFormat hrefNameFormat;
    hrefNameFormat.setForeground(QColor("blue"));
    hrefNameFormat.setFontUnderline(true);
    styledTagsMap.insert("a.name", { "a.name", hrefNameFormat });

    QTextCharFormat hrefAttrFormat;
    hrefAttrFormat.setForeground(Qt::blue);
    hrefAttrFormat.setFontPointSize(9);
    styledTagsMap.insert("a.href", { "a.href", hrefAttrFormat });

    // Obrazki (img src=...)
    const QColor mint{"#2c7"};
    QTextCharFormat imgSrcFormat;
    imgSrcFormat.setForeground(mint);
    imgSrcFormat.setFontPointSize(9);
    styledTagsMap.insert("img.src", { "img.src", imgSrcFormat });

    QTextCharFormat imgAltFormat;
    imgAltFormat.setForeground(Qt::darkGreen);
    imgAltFormat.setFontPointSize(10);
    imgAltFormat.setFontItalic(true);
    styledTagsMap.insert("img.alt", { "img.alt", imgAltFormat });

    QTextCharFormat imgOpisFormat;
    imgOpisFormat.setForeground(mint);
    imgOpisFormat.setFontItalic(true);
    styledTagsMap.insert("img.opis", { "img.opis", imgOpisFormat });

    QTextCharFormat autofitFormat;
    autofitFormat.setFontWeight(QFont::Bold);
    autofitFormat.setForeground(QColor("darkgray"));
    imgAltFormat.setFontPointSize(10);
    styledTagsMap.insert("img.autofit", {"img.autofit", autofitFormat});

    // Styl ogólny tagu [img ...] lub [a ...]
    QTextCharFormat tagFmt;
    tagFmt.setForeground(Qt::gray);
    tagFmt.setFontPointSize(8);
    styledTagsMap.insert("tag.attr", { "tag.attr", tagFmt });
}

void STCSyntaxHighlighter::highlightBlock(const QString &text)
{
    _codeRangesThisLine.clear();     // czyszczenie przed każdą linią
    _noFormatRangesThisLine.clear(); // czyszczenie przed każdą linią

    const int prev = previousBlockState();  // zapisz zanim nadpiszesz
    DEBUG(true, "----------") << prev << text;

    // --- 1. DIV (najbardziej zewnętrzny) ---
    bool divChanges = highlightDivBlock(text);
    DEBUG(divChanges, "div");

    // --- 2. Nagłówki ---
    bool headersChanges = highlightHeading(text);
    DEBUG(divChanges, "hN");

    // // --- 3. pkt / csv (mogą zawierać inne tagi) ---
    bool pktOrCsvChanges = highlightPktOrCsv(text);
    if (pktOrCsvChanges)
    {
        DEBUG(pktOrCsvChanges, "pkt/csv");
        bool divChanges = highlightDivBlock(text);
        DEBUG(divChanges, "div");
    }

    // // --- 4. Bloki kodu ---
    bool codeChanges = highlightCodeBlock(text);
    DEBUG(codeChanges, "code");

    // --- 5. Stylizacja tekstu (b/i/u/s) ---
    bool styleChanges = highlightTextStyleTags(text);
    DEBUG(styleChanges, "style");

    // --- 6. Tagi z atrybutami [a href=...] ---
    bool hrefImgChanges = highlightTagsWithAttributes(text);
    DEBUG(hrefImgChanges, "href/img");

    // --- 7. Spellcheck for untagged plain text ---
    highlightPlainTextContent(text);

    bool anyChange = divChanges | headersChanges | pktOrCsvChanges | codeChanges | styleChanges | hrefImgChanges;
    if (!anyChange)
        setCurrentBlockState(prev);
}

bool STCSyntaxHighlighter::highlightHeading(const QString &text)
{
    bool found = false;

    for (const auto& styled : styledTags)
    {
        if (!styled.tag.startsWith("h"))
            continue;

        QRegularExpression re(QStringLiteral(R"(\[(%1)\](.*?)\[/\1\])").arg(styled.tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext())
        {
            auto match = it.next();
            const int fullStart = match.capturedStart(0);
            const int contentStart = match.capturedStart(2);
            const int contentLen = match.capturedLength(2);
            const int fullEnd = match.capturedEnd(0);

            // Format heading content
            setFormat(contentStart, contentLen, styled.format);

            // Format heading tags ([h1], [/h1])
            QTextCharFormat tagFmt;
            tagFmt.setForeground(Qt::gray);
            tagFmt.setFontPointSize(8);
            setFormat(fullStart, contentStart - fullStart, tagFmt);                              // opening tag
            setFormat(contentStart + contentLen, fullEnd - (contentStart + contentLen), tagFmt); // closing tag

            // Apply spellcheck to the content
            applySpellcheckToTextRange(text, contentStart, contentLen, styled.format);

            found = true;
        }
    }
    return found;
} // TODO: Why not to find all headers at once, but searching text 6 times?

bool STCSyntaxHighlighter::highlightDivBlock(const QString &text)
{
    static QTextCharFormat tagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    const int prevState = previousBlockState();
    bool foundAny = false;

    // === 1. Handle continuation of multi-line div or quote blocks ===
    if (prevState != STATE_NONE)
    {
        if (prevState & (DIV_CLASS_TIP | DIV_CLASS_UWAGA | DIV_CLASS_PLAIN | DIV_CLASS_CYTAT))
        {
            QTextCharFormat fmt;
            QString closingTag = "[/div]";
            if (prevState == DIV_CLASS_TIP)
                fmt = styledTagsMap.value("tip").format;
            else if (prevState == DIV_CLASS_UWAGA)
                fmt = styledTagsMap.value("warning").format;
            else if (prevState == DIV_CLASS_CYTAT)
            {
                fmt = styledTagsMap.value("cytat").format;
                closingTag = "[/cytat]";
            }
            else
                fmt = styledTagsMap.value("div").format;

            QRegularExpressionMatch closeMatch = stc::syntax::divCloseRe.match(text);
            if (closeMatch.hasMatch())
            {
                int closeStart = closeMatch.capturedStart();
                int contentLen = closeStart;

                // if (overlapsWithNoFormat(closeStart, closingTag.size())) // TODO:
                //     return false;

                // Format body
                setFormat(0, contentLen, fmt);

                // Apply spellcheck on body
                applySpellcheckToTextRange(text, 0, contentLen, fmt);

                // Format closing tag
                setFormat(closeStart, closeMatch.capturedLength(0), tagFmt);
                currentBlockStateWithoutFlag(prevState);
            } else {
                setFormat(0, text.length(), fmt);
                applySpellcheckToTextRange(text, 0, text.length(), fmt);
                setCurrentBlockState(prevState);
            }

            return true;
        }
    }

    // === 2. Look for new div/cytat blocks in this line ===
    QRegularExpressionMatchIterator it = stc::syntax::divOpenRe.globalMatch(text);
    while (it.hasNext())
    {
        QRegularExpressionMatch openMatch = it.next();
        int tagStart = openMatch.capturedStart(0);
        int tagEnd = openMatch.capturedEnd(0);

        const QString tagName = openMatch.captured(1); // "div"
        const QString divClass = openMatch.captured(2); // "tip" / "uwaga"
        const QString quoteTag = openMatch.captured(3); // "cytat"

        QTextCharFormat fmt;
        int blockState = DIV_CLASS_PLAIN;

        if (tagName == "div")
        {
            if (divClass == "tip")
            {
                fmt = styledTagsMap.value("tip").format;
                blockState = DIV_CLASS_TIP;
            }
            else if (divClass == "uwaga")
            {
                fmt = styledTagsMap.value("warning").format;
                blockState = DIV_CLASS_UWAGA;
            }
            else
            {
                fmt = styledTagsMap.value("div").format;
                blockState = DIV_CLASS_PLAIN;
            }
        }
        else if (!quoteTag.isEmpty())
        {
            fmt = styledTagsMap.value("cytat").format;
            blockState = DIV_CLASS_CYTAT;
        }

        QRegularExpressionMatch closeMatch = stc::syntax::divCloseRe.match(text, tagEnd);
        if (closeMatch.hasMatch())
        {
            // One-line block
            int contentStart = tagEnd;
            int contentEnd = closeMatch.capturedStart(0);
            int contentLen = contentEnd - contentStart;

            // Format opening tag
            setFormat(tagStart, tagEnd - tagStart, tagFmt);

            // Format and spellcheck body
            setFormat(contentStart, contentLen, fmt);
            applySpellcheckToTextRange(text, contentStart, contentLen, fmt);

            // Format closing tag
            setFormat(closeMatch.capturedStart(0), closeMatch.capturedLength(0), tagFmt);
            currentBlockStateWithFlag(prevState);
        }
        else
        {
            // Multi-line block start
            setFormat(tagStart, tagEnd - tagStart, tagFmt);
            setFormat(tagEnd, text.length() - tagEnd, fmt);
            applySpellcheckToTextRange(text, tagEnd, text.length() - tagEnd, fmt);
            currentBlockStateWithFlag(blockState);
        }

        foundAny = true;
    }

    return foundAny;
} // TODO: Divy zagnieżdżone jakoś trzeba obsłużyć
void STCSyntaxHighlighter::applySpellcheckToTextRange(const QString& text, int start, int length, const QTextCharFormat& baseFormat)
{
    const QString innerText = text.mid(start, length);
    QRegularExpressionMatchIterator it = stc::syntax::wordWithPolishCharactersRe.globalMatch(innerText);

    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const QString word = match.captured();
        const int wordStart = start + match.capturedStart();
        const int wordLen = match.capturedLength();

        if (overlapsWithCode(wordStart, wordLen) || overlapsWithNoFormat(wordStart, wordLen))
            continue;

        if (!spellChecker.isCorrect(word))
        {
            QTextCharFormat errFmt = baseFormat;
            errFmt.setUnderlineColor(Qt::red);
            errFmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
            setFormat(wordStart, wordLen, errFmt);
        }
    }
}

bool STCSyntaxHighlighter::highlightPktOrCsv(const QString& text)
{
    static const QTextCharFormat runTagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    const int prevState = previousBlockState();
    bool foundAny = false;

    // --- 1. Continuation of multiline pkt/csv block ---
    if (prevState != STATE_NONE)
    {
        auto handleBlock = [&](int stateFlag, const QString& tagName, const QRegularExpression& closeRe) {
            if (!(prevState & stateFlag))
                return;

            QRegularExpressionMatch close = closeRe.match(text);
            QTextCharFormat fmt = styledTagsMap.value(tagName).format;

            if (close.hasMatch())
            {
                int closeStart = close.capturedStart();
                setFormat(0, closeStart, fmt);
                applySpellcheckToTextRange(text, 0, closeStart, fmt);
                setFormat(closeStart, close.capturedLength(), runTagFmt);
                currentBlockStateWithoutFlag(stateFlag);
            }
            else
            {
                setFormat(0, text.length(), fmt);
                applySpellcheckToTextRange(text, 0, text.length(), fmt);
                currentBlockStateWithFlag(stateFlag);
            }

            foundAny = true;
        };

        handleBlock(STATE_PKT, "pkt", stc::syntax::pktCloseRe);
        handleBlock(STATE_CSV, "csv", stc::syntax::csvCloseRe);

        // Format [run] only if in extended mode
        bool extendedMode = text.contains("ext") || text.contains("extended") || text.contains("extended header");

        auto runMatches = stc::syntax::runTagRe.globalMatch(text);
        while (runMatches.hasNext())
        {
            auto m = runMatches.next();
            setFormat(m.capturedStart(), 5, runTagFmt); // [run]
            setFormat(m.capturedEnd() - 6, 6, runTagFmt); // [/run]

            if (extendedMode)
            {
                _noFormatRangesThisLine.append({ m.capturedStart(1), m.capturedLength(1) });
            }
        }

        if (foundAny)
            return true;
    }

    // --- 2. New openings on the same line ---
    auto processTag = [&](const QRegularExpression& openRe,
                          const QRegularExpression& closeRe,
                          const QString& tagName,
                          int stateFlag)
    {
        QTextCharFormat fmt = styledTagsMap.value(tagName).format;

        QRegularExpressionMatchIterator it = openRe.globalMatch(text);
        while (it.hasNext())
        {
            QRegularExpressionMatch open = it.next();
            int tagStart = open.capturedStart();
            int tagEnd = open.capturedEnd();

            // If tag or its content overlaps with excluded area — skip
            int totalEnd = text.length(); // in case of no closing tag
            QRegularExpressionMatch close = closeRe.match(text, tagEnd);
            if (close.hasMatch())
                totalEnd = close.capturedEnd();

            // if (overlapsWithNoFormat(tagStart, totalEnd - tagStart)) // TODO:
            //     continue;

            setFormat(tagStart, tagEnd - tagStart, runTagFmt);

            if (close.hasMatch())
            {
                int contentStart = tagEnd;
                int contentEnd = close.capturedStart();
                int contentLen = contentEnd - contentStart;

                setFormat(contentStart, contentLen, fmt);
                applySpellcheckToTextRange(text, contentStart, contentLen, fmt);
                setFormat(close.capturedStart(), close.capturedLength(), runTagFmt);
            }
            else
            {
                setFormat(tagEnd, text.length() - tagEnd, fmt);
                applySpellcheckToTextRange(text, tagEnd, text.length() - tagEnd, fmt);
                currentBlockStateWithFlag(stateFlag);
            }

            foundAny = true;
        }
    };

    processTag(stc::syntax::pktOpenRe, stc::syntax::pktCloseRe, "pkt", STATE_PKT);
    processTag(stc::syntax::csvOpenRe, stc::syntax::csvCloseRe, "csv", STATE_CSV);

    return foundAny;
} // TODO: Dodać ignorowanie, gdy tagi nie wewnątrz `[run]`

void STCSyntaxHighlighter::currentBlockStateWithoutFlag(int flag, std::source_location location)
{
    const auto previous = previousBlockState();
    if (previous == flag)
    {
        DEBUG(true, "\t-") << "\t usuwamy poprzednia flage: " << flag << ". linia:" << location.line();
        setCurrentBlockState(STATE_NONE);
    }
    else
    {
        const auto newState = previous & ~flag;
        DEBUG(true, "\t-") << "\t usuwamy ze stanu " << previous << " flage: " << flag << ", nowy stan: " << newState << ". linia:" << location.line();
        setCurrentBlockState(newState);
    }
}
void STCSyntaxHighlighter::currentBlockStateWithFlag(int flag, std::source_location location)
{
    const auto previous = previousBlockState();
    if (STATE_NONE == previous)
    {
        DEBUG(true, "\t+")  << "\t dodaje stan flage: " << flag << ". linia:" << location.line();
        setCurrentBlockState(flag);
    }
    else
    {
        const auto newState = previous | flag;
        DEBUG(true, "\t+") << "\t dodaje do stanu " << previous << " flage: " << flag << ", nowy stan: " << newState << ". linia:" << location.line();
        setCurrentBlockState(newState);
    }
}

bool STCSyntaxHighlighter::highlightCodeBlock(const QString& text)
{
    static const QMap<QString, QRegularExpression> closeReMap =
    {
        { "cpp",  stc::syntax::cppCloseRe },
        { "py",   stc::syntax::pythonCloseRe },
        { "code", stc::syntax::codeCloseRe }
    };

    static QTextCharFormat tagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    int offset = 0;
    bool found = false;

    const int prev = previousBlockState();
    if (prev != STATE_NONE)
    {
        struct CodeBlock
        {
            int stateFlag;
            QString styleKey;
            QRegularExpression closeRe;
        };

        const QVector<CodeBlock> blocks =
        {
            { STATE_CODE_CPP, "cpp", stc::syntax::cppCloseRe },
            { STATE_CODE,     "code", stc::syntax::codeCloseRe },
            { STATE_CPP,      "py", stc::syntax::pythonCloseRe }
        };

        for (const auto& blk : blocks)
        {
            if (prev & blk.stateFlag)
            {
                QTextCharFormat fmt = styledTagsMap.value(blk.styleKey).format;
                QRegularExpressionMatch close = blk.closeRe.match(text);
                if (close.hasMatch())
                {
                    int closeStart = close.capturedStart();

                    // if (overlapsWithNoFormat(closeStart, text.size())) // TODO:
                    //     continue;

                    setFormat(0, closeStart, fmt);
                    setFormat(closeStart, close.capturedLength(), tagFmt);
                    _codeRangesThisLine.append({ closeStart, close.capturedLength() });
                    currentBlockStateWithoutFlag(blk.stateFlag);
                    offset = close.capturedEnd();
                    found = true;
                    continue; // nie break – idź dalej z analizą!
                }
                else
                {
                    setFormat(0, text.length(), fmt);
                    currentBlockStateWithFlag(blk.stateFlag);
                    _codeRangesThisLine.append({ 0, text.length() });
                    return true; // nadal jesteśmy w wieloliniowym bloku
                }
            }
        }
    }

    while (offset < text.length())
    {
        QRegularExpressionMatch openMatch = stc::syntax::codeBlockOpenRe.match(text, offset);
        if (!openMatch.hasMatch())
            break;

        const QString tag = openMatch.captured(1);
        const QString src = openMatch.captured(2).toLower();
        const int tagStart = openMatch.capturedStart();
        const int tagEnd = openMatch.capturedEnd();

        QTextCharFormat fmt;
        QString styleKey;
        int stateFlag;
        QRegularExpression closeRe;

        if (tag == "cpp" || src.contains("cpp") || src.contains("c++"))
        {
            styleKey = "cpp";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CODE_CPP;
            closeRe = closeReMap["cpp"];
        }
        else if (tag == "py")
        {
            styleKey = "py";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CPP;
            closeRe = closeReMap["py"];
        }
        else
        {
            styleKey = "code";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CODE;
            closeRe = closeReMap["code"];
        }

        QRegularExpressionMatch closeMatch = closeRe.match(text, tagEnd);
        if (closeMatch.hasMatch())
        {
            // Kod inline
            const int closeStart = closeMatch.capturedStart();
            const int contentStart = tagEnd;
            const int contentLen = closeStart - contentStart;
            const int closeLen = closeMatch.capturedLength();

            offset = closeMatch.capturedEnd();
            // if (overlapsWithNoFormat(closeStart, closeLen)) // TODO:
            //     continue;

            setFormat(tagStart, tagEnd - tagStart, tagFmt);
            setFormat(contentStart, contentLen, fmt);
            _codeRangesThisLine.append({ contentStart, contentLen });
            setFormat(closeStart, closeLen, tagFmt);

            found = true;
        }
        else
        {
            // Rozpoczęcie wieloliniowego bloku
            setFormat(tagStart, tagEnd - tagStart, tagFmt);
            setFormat(tagEnd, text.length() - tagEnd, fmt);
            _codeRangesThisLine.append({ tagEnd, text.length() - tagEnd });
            currentBlockStateWithFlag(stateFlag);
            return true;
        }
    }

    return found;
}

bool STCSyntaxHighlighter::highlightTextStyleTags(const QString& text)
{
    const int prev = previousBlockState();

    static const QMap<QString, QTextCharFormat> tagFormats = [] {
        QMap<QString, QTextCharFormat> map;
        QTextCharFormat f;

        f.setFontWeight(QFont::Bold); map["b"] = f;
        f = QTextCharFormat(); f.setFontItalic(true); map["i"] = f;
        f = QTextCharFormat(); f.setFontUnderline(true); map["u"] = f;
        f = QTextCharFormat(); f.setFontStrikeOut(true); map["s"] = f;

        return map;
    }();

    static const QMap<QString, int> tagStates =
    {
        { "b", STATE_STYLE_BOLD },
        { "i", STATE_STYLE_ITALIC },
        { "u", STATE_STYLE_UNDERLINE },
        { "s", STATE_STYLE_STRIKE },
    };

    static const QMap<QString, QRegularExpression> closeRes =
    {
        { "b", stc::syntax::boldCloseRe },
        { "i", stc::syntax::italicCloseRe },
        { "u", stc::syntax::underlineCloseRe },
        { "s", stc::syntax::strikeOutCloseRe },
    };

    static QTextCharFormat tagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    // --- 1. Continuation of a multiline style ---
    if (prev != STATE_NONE)
    {
        for (auto it = tagStates.constBegin(); it != tagStates.constEnd(); ++it)
        {
            const QString& tag = it.key();
            const int flag = it.value();

            if (prev & flag)
            {
                const QRegularExpression& closeRe = closeRes[tag];
                QTextCharFormat fmt = tagFormats[tag];

                QRegularExpressionMatch closeMatch = closeRe.match(text);
                if (closeMatch.hasMatch())
                {
                    int closeStart = closeMatch.capturedStart();

                    // If style closing is inside code — skip the entire style
                    if (overlapsWithCode(0, closeStart + closeMatch.capturedLength()))
                        return false;
                    if (overlapsWithNoFormat(0, closeStart + closeMatch.capturedLength()))
                        return false;

                    setFormat(0, closeStart, fmt);
                    applySpellcheckToTextRange(text, 0, closeStart, fmt);
                    setFormat(closeStart, closeMatch.capturedLength(), tagFmt);
                    currentBlockStateWithoutFlag(flag);
                }
                else
                {
                    // Entire line styled — only if not overlapping with code
                    if (!overlapsWithCode(0, text.length()) && !overlapsWithNoFormat(0, text.length()))
                    {
                        setFormat(0, text.length(), fmt);
                        applySpellcheckToTextRange(text, 0, text.length(), fmt);
                        currentBlockStateWithFlag(flag);
                    }
                }
                return true;
            }
        }
    }

    // --- 2. Single-line style (can be multiple per line) ---
    int offset = 0;
    bool foundAny = false;

    while (offset < text.length())
    {
        QRegularExpressionMatch match = stc::syntax::baseFormatting_boldItalicUnderlineStrikeRe.match(text, offset);
        if (!match.hasMatch())
            break;

        const QString tag = match.captured(1);
        const QTextCharFormat fmt = tagFormats[tag];
        const int flag = tagStates[tag];
        const QRegularExpression& closeRe = closeRes[tag];

        const int tagStart = match.capturedStart();
        const int tagEnd = match.capturedEnd();

        QRegularExpressionMatch closeMatch = closeRe.match(text, tagEnd);
        if (closeMatch.hasMatch()) {
            const int closeStart = closeMatch.capturedStart();
            const int closeEnd = closeMatch.capturedEnd();
            const int contentStart = tagEnd;
            const int contentLen = closeStart - contentStart;

            // IGNORE styled tags inside code
            if (overlapsWithCode(tagStart, closeEnd - tagStart) || overlapsWithNoFormat(tagStart, closeEnd - tagStart))
            {
                offset = closeEnd;
                continue;
            }

            setFormat(tagStart, tagEnd - tagStart, tagFmt);           // [b]
            setFormatKeepingBackground(contentStart, contentLen, fmt);                 // content
            applySpellcheckToTextRange(text, contentStart, contentLen, fmt);
            setFormat(closeStart, closeEnd - closeStart, tagFmt);     // [/b]

            offset = closeEnd;
            foundAny = true;
        }
        else
        {
            // Start of multiline style — only if not in code
            if (!overlapsWithCode(tagStart, text.length() - tagStart) && !overlapsWithNoFormat(tagStart, text.length() - tagStart))
            {
                setFormat(tagStart, tagEnd - tagStart, tagFmt);
                setFormatKeepingBackground(tagEnd, text.length() - tagEnd, fmt);
                applySpellcheckToTextRange(text, tagEnd, text.length() - tagEnd, fmt);
                currentBlockStateWithFlag(flag);
                return true;
            }
            else
            {
                // If inside code — skip
                offset = tagEnd;
            }
        }
    }

    return foundAny;
}
void STCSyntaxHighlighter::setFormatKeepingBackground(int contentStart, int contentLen, const QTextCharFormat &format)
{
    for (int i = contentStart; i < contentStart + contentLen; ++i)
    {
        QTextCharFormat baseFmt = this->format(i);
        baseFmt.setFontWeight(format.fontWeight());
        baseFmt.setFontItalic(format.fontItalic());
        baseFmt.setFontUnderline(format.fontUnderline());
        baseFmt.setFontStrikeOut(format.fontStrikeOut());
        setFormat(i, 1, baseFmt);
    }
}

bool STCSyntaxHighlighter::overlapsWithRange(int start, int length, const QVector<QPair<int, int>>& range)
{
    for (const auto& range : range)
    {
        const int a1 = start;
        const int a2 = start + length;
        const int b1 = range.first;
        const int b2 = range.first + range.second;
        if (a1 < b2 && b1 < a2)
            return true;
    }
    return false;
}

bool STCSyntaxHighlighter::highlightTagsWithAttributes(const QString& text)
{
    bool found = false;

    // Find all anchor tags
    auto anchorIt = stc::syntax::anchorRe.globalMatch(text);
    while (anchorIt.hasNext())
    {
        auto match = anchorIt.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        if (overlapsWithNoFormat(start, end - start))
            continue;

        // Format the full tag
        setFormat(start, end - start, styledTagsMap.value("tag.attr").format);

        // Format href attribute value
        int hrefStart = match.capturedStart(1);
        int hrefLen = match.capturedLength(1);
        setFormat(hrefStart, hrefLen, styledTagsMap.value("a.href").format);

        // Format name attribute value, if present
        if (match.lastCapturedIndex() >= 2 && match.captured(2).length() > 0)
        {
            int nameStart = match.capturedStart(2);
            int nameLen = match.capturedLength(2);
            setFormat(nameStart, nameLen, styledTagsMap.value("a.name").format);
            applySpellcheckToTextRange(text, nameStart, nameLen, styledTagsMap.value("a.name").format);
        }

        found = true;
    }

    // Find all image tags
    auto imgIt = stc::syntax::imgRe.globalMatch(text);
    while (imgIt.hasNext())
    {
        auto match = imgIt.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        if (overlapsWithNoFormat(start, end - start))
            continue;

        // Format the entire [img ...] block
        setFormat(start, end - start, styledTagsMap.value("tag.attr").format);

        QString matchedText = match.captured(0);

        // Find and format src attribute
        auto srcMatch = stc::syntax::imgAttributeSrcRe.match(matchedText);
        if (srcMatch.hasMatch())
        {
            int srcStart = start + srcMatch.capturedStart(1);
            int srcLen = srcMatch.capturedLength(1);
            setFormat(srcStart, srcLen, styledTagsMap.value("img.src").format);
        }

        // Find and format alt attribute
        auto altMatch = stc::syntax::imgAttributeSrcRe.match(matchedText);
        if (altMatch.hasMatch())
        {
            int altStart = start + altMatch.capturedStart(1);
            int altLen = altMatch.capturedLength(1);
            setFormat(altStart, altLen, styledTagsMap.value("img.alt").format);
        }

        // Find and format opis attribute
        auto opisMatch = stc::syntax::imgAttributeDescRe.match(matchedText);
        if (opisMatch.hasMatch())
        {
            int opisStart = start + opisMatch.capturedStart(1);
            int opisLen = opisMatch.capturedLength(1);
            setFormat(opisStart, opisLen, styledTagsMap.value("img.opis").format);
            applySpellcheckToTextRange(text, opisStart, opisLen, styledTagsMap.value("img.opis").format);
        }

        // Find and format 'autofit' keyword
        auto autofitMatch = stc::syntax::imgAttributeAutofitRe.match(matchedText);
        if (autofitMatch.hasMatch())
        {
            int autofitStart = start + autofitMatch.capturedStart();
            int autofitLen = autofitMatch.capturedLength();
            setFormat(autofitStart, autofitLen, styledTagsMap.value("img.autofit").format);
        }

        found = true;
    }

    return found;
}

void STCSyntaxHighlighter::highlightPlainTextContent(const QString& text)
{
    const int length = text.length();
    QVector<QPair<int, int>> excludedRanges = _codeRangesThisLine + _noFormatRangesThisLine;

    // Exclude also tag-like segments: [tag] or [/tag]
    static const QRegularExpression tagRe(R"(\[\/?\w+.*?\])");
    QRegularExpressionMatchIterator tagIt = tagRe.globalMatch(text);
    while (tagIt.hasNext())
    {
        auto m = tagIt.next();
        excludedRanges.append({ m.capturedStart(), m.capturedLength() });
    }

    // Sort and merge excluded ranges to avoid overlap
    std::sort(excludedRanges.begin(), excludedRanges.end());
    QVector<QPair<int, int>> mergedRanges;
    for (const auto& range : excludedRanges)
    {
        if (mergedRanges.isEmpty()) {
            mergedRanges.append(range);
        }
        else
        {
            auto& last = mergedRanges.last();
            int lastEnd = last.first + last.second;
            if (range.first <= lastEnd)
            {
                // Overlapping ranges: extend
                last.second = std::max(lastEnd, range.first + range.second) - last.first;
            }
            else
            {
                mergedRanges.append(range);
            }
        }
    }

    // Apply spellchecking between excluded ranges
    int current = 0;
    for (const auto& range : mergedRanges)
    {
        int start = current;
        int end = range.first;
        if (end > start)
            applySpellcheckToTextRange(text, start, end - start, format(start));

        current = std::max(current, range.first + range.second);
    }

    // Check remaining tail
    if (current < length)
        applySpellcheckToTextRange(text, current, length - current, format(current));
}

void STCSyntaxHighlighter::addBlockStyle(const QString &tag,
                                         QColor foreground,
                                         std::uint64_t format,
                                         int pointSize,
                                         QColor background,
                                         const QString &fontFamily)
{
    QTextCharFormat fmt;
    if (foreground.isValid())
        fmt.setForeground(foreground);
    if (background.isValid())
        fmt.setBackground(background);
    if (format & std::to_underlying(StcTags::BOLD))
        fmt.setFontWeight(QFont::Bold);
    if (format & std::to_underlying(StcTags::ITALIC))
        fmt.setFontItalic(true);
    if (format & std::to_underlying(StcTags::UNDERLINED))
        fmt.setFontUnderline(true);
    if (format & std::to_underlying(StcTags::STRUCK_OUT))
        fmt.setFontStrikeOut(true);
    if (format & std::to_underlying(StcTags::SUBSCRIPT))
        fmt.setVerticalAlignment(QTextCharFormat::AlignSubScript); // probably it will not work until I use QTextEdit as base
    if (format & std::to_underlying(StcTags::SUPSCRIPT))
        fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript); // probably it will not work until I use QTextEdit as base
    if (format & std::to_underlying(StcTags::TELE_TYPE))
    {
        fmt.setFontFixedPitch(true);
        fmt.setFontStyleHint(QFont::Monospace);
        fmt.setFontFamilies({"Monospace"});
    }
    if (pointSize > 0)
        fmt.setFontPointSize(pointSize);
    if (!fontFamily.isEmpty())
        fmt.setFontFamilies({fontFamily});

    styledTags.append({ tag, fmt });
    styledTagsMap.insert(tag, { tag, fmt });

    styledTags.append({ tag, fmt });
}
