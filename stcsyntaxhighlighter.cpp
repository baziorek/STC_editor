#include <QDebug>
#include <QRegularExpression>
#include "stcsyntaxhighlighter.h"
#include "types/stcTags.h"

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
#warning "Chat helped me with the code, but it requires refactoring and corrections"
} // namespace


STCSyntaxHighlighter::STCSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    using enum StcTags;

    // Styl tagów (np. [code], [/div] itd.)
    QTextCharFormat tagFormat;
    tagFormat.setForeground(Qt::gray);
    tagFormat.setFontPointSize(8);
    rules.append({ QRegularExpression(R"(\[/?\w+(=[^\]]+)?\])"), tagFormat });

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

    bool anyChange = divChanges | headersChanges | pktOrCsvChanges | codeChanges | styleChanges | hrefImgChanges;
    if (!anyChange)
        setCurrentBlockState(prev);
}

bool STCSyntaxHighlighter::highlightHeading(const QString &text)
{
    for (const auto& styled : styledTags)
    {
        if (styled.tag.startsWith("h")) {
            QRegularExpression re(QStringLiteral("\\[(%1)\\](.*?)\\[/\\1\\]").arg(styled.tag));
            auto it = re.globalMatch(text);
            while (it.hasNext()) {
                auto match = it.next();
                const int start = match.capturedStart(2);
                const int len = match.capturedLength(2);

                // if (overlapsWithNoFormat(start, len)) // TODO:
                //     continue;

                setFormat(start, len, styled.format);

                QTextCharFormat tagFmt;
                tagFmt.setForeground(Qt::gray);
                tagFmt.setFontPointSize(8);
                setFormat(match.capturedStart(0), start - match.capturedStart(0), tagFmt);
                setFormat(start + len, match.capturedEnd(0) - (start + len), tagFmt);
            }
        }
    }
    return false;
}

bool STCSyntaxHighlighter::highlightDivBlock(const QString &text)
{
    static const QRegularExpression divOpenRe(R"___(\[(div)(?:\s+class="(tip|uwaga)")?\]|\[(cytat)\])___");
    static const QRegularExpression divCloseRe(R"___(\[/div\]|\[/cytat\])___");

    static QTextCharFormat tagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    const int prevState = previousBlockState();

    // --- 1. Kontynuacja wieloliniowego DIV-a lub cytatu ---
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

            QRegularExpressionMatch closeMatch = divCloseRe.match(text);
            if (closeMatch.hasMatch()) {
                int closeStart = closeMatch.capturedStart(0);

                // if (overlapsWithNoFormat(closeStart, closingTag.size())) // TODO:
                //     return false;

                setFormat(0, closeStart, fmt);
                setFormat(closeStart, closeMatch.capturedLength(0), tagFmt);
                currentBlockStateWithoutFlag(prevState);
            } else {
                setFormat(0, text.length(), fmt);
                setCurrentBlockState(prevState);  // nadal jesteśmy w środku
            }
            return true;
        }
    }

    // --- 2. Wyszukiwanie nowych DIV-ów lub cytatów w linii ---
    bool foundAny = false;
    QRegularExpressionMatchIterator it = divOpenRe.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch openMatch = it.next();
        int tagStart = openMatch.capturedStart(0);
        int tagEnd = openMatch.capturedEnd(0);

        const QString tagName = openMatch.captured(1); // "div"
        const QString divClass = openMatch.captured(2); // "tip" / "uwaga"
        const QString quoteTag = openMatch.captured(3); // "cytat"

        QTextCharFormat fmt;
        int blockState = DIV_CLASS_PLAIN;

        if (tagName == "div") {
            if (divClass == "tip") {
                fmt = styledTagsMap.value("tip").format;
                blockState = DIV_CLASS_TIP;
            } else if (divClass == "uwaga") {
                fmt = styledTagsMap.value("warning").format;
                blockState = DIV_CLASS_UWAGA;
            } else {
                fmt = styledTagsMap.value("div").format;
                blockState = DIV_CLASS_PLAIN;
            }
        } else if (!quoteTag.isEmpty()) {
            fmt = styledTagsMap.value("cytat").format;
            blockState = DIV_CLASS_CYTAT;
        }

        QRegularExpressionMatch closeMatch = divCloseRe.match(text, tagEnd);
        if (closeMatch.hasMatch()) {
            // Zamknięcie w tej samej linii
            int contentStart = tagEnd;
            int contentEnd = closeMatch.capturedStart(0);
            int contentLen = contentEnd - contentStart;

            setFormat(tagStart, tagEnd - tagStart, tagFmt);                 // [div] lub [cytat]
            setFormat(contentStart, contentLen, fmt);                       // zawartość
            setFormat(closeMatch.capturedStart(0), closeMatch.capturedLength(0), tagFmt);  // [/div] lub [/cytat]
            currentBlockStateWithFlag(prevState);
        } else {
            // Początek wieloliniowego bloku
            setFormat(tagStart, tagEnd - tagStart, tagFmt);
            setFormat(tagEnd, text.length() - tagEnd, fmt);
            currentBlockStateWithFlag(blockState);
        }

        foundAny = true;
    }

    return foundAny;
} // TODO: Divy zagnieżdżone jakoś trzeba obsłużyć

bool STCSyntaxHighlighter::highlightPktOrCsv(const QString& text)
{
    static const QRegularExpression pktOpenRe(R"(\[pkt(\s+[^\]]*)?\])");
    static const QRegularExpression pktCloseRe(R"(\[/pkt\])");

    static const QRegularExpression csvOpenRe(R"(\[csv(\s+[^\]]*)?\])");
    static const QRegularExpression csvCloseRe(R"(\[/csv\])");

    static const QRegularExpression runTagRe(R"(\[run\](.*?)\[/run\])");
    static const QTextCharFormat runTagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    const int prevState = previousBlockState();
    bool foundAny = false;

    // --- 1. Kontynuacja wieloliniowego bloku pkt/csv ---
    if (prevState != STATE_NONE)
    {
        auto handleBlock = [&](int stateFlag, const QString& tagName, const QRegularExpression& closeRe) {
            if (!(prevState & stateFlag))
                return;

            QRegularExpressionMatch close = closeRe.match(text);
            QTextCharFormat fmt = styledTagsMap.value(tagName).format;

            if (close.hasMatch()) {
                int closeStart = close.capturedStart();
                setFormat(0, closeStart, fmt);
                setFormat(closeStart, close.capturedLength(), runTagFmt);
                currentBlockStateWithoutFlag(stateFlag);
            } else {
                setFormat(0, text.length(), fmt);
                currentBlockStateWithFlag(stateFlag);
            }

            foundAny = true;
        };

        handleBlock(STATE_PKT, "pkt", pktCloseRe);
        handleBlock(STATE_CSV, "csv", csvCloseRe);

        // Formatowanie [run] tylko jeśli tryb rozszerzony
        bool extendedMode = text.contains("ext") || text.contains("extended") || text.contains("extended header");

        auto runMatches = runTagRe.globalMatch(text);
        while (runMatches.hasNext()) {
            auto m = runMatches.next();
            setFormat(m.capturedStart(), 5, runTagFmt); // [run]
            setFormat(m.capturedEnd() - 6, 6, runTagFmt); // [/run]

            if (extendedMode) {
                _noFormatRangesThisLine.append({ m.capturedStart(1), m.capturedLength(1) });
            }
        }

        if (foundAny)
            return true;
    }

    // --- 2. Nowe otwarcia w tej samej linii ---
    auto processTag = [&](const QRegularExpression& openRe,
                          const QRegularExpression& closeRe,
                          const QString& tagName,
                          int stateFlag)
    {
        QTextCharFormat fmt = styledTagsMap.value(tagName).format;

        QRegularExpressionMatchIterator it = openRe.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch open = it.next();
            int tagStart = open.capturedStart();
            int tagEnd = open.capturedEnd();

            // Jeśli cały tag (lub jego zawartość) nachodzi na zakres wykluczony — pomijamy
            int totalEnd = text.length(); // w razie braku zamknięcia
            QRegularExpressionMatch close = closeRe.match(text, tagEnd);
            if (close.hasMatch())
                totalEnd = close.capturedEnd();

            // if (overlapsWithNoFormat(tagStart, totalEnd - tagStart)) // TODO:
            //     continue;

            setFormat(tagStart, tagEnd - tagStart, runTagFmt);

            if (close.hasMatch()) {
                int contentStart = tagEnd;
                int contentEnd = close.capturedStart();
                int contentLen = contentEnd - contentStart;

                setFormat(contentStart, contentLen, fmt);
                setFormat(close.capturedStart(), close.capturedLength(), runTagFmt);
            } else {
                setFormat(tagEnd, text.length() - tagEnd, fmt);
                currentBlockStateWithFlag(stateFlag);
            }

            foundAny = true;
        }
    };

    processTag(pktOpenRe, pktCloseRe, "pkt", STATE_PKT);
    processTag(csvOpenRe, csvCloseRe, "csv", STATE_CSV);

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
    static const QRegularExpression openRe(R"__(\[(cpp|py|code)(?:\s+src="([^"]+)")?\])__");
    static const QMap<QString, QRegularExpression> closeReMap = {
        { "cpp",  QRegularExpression(R"(\[/cpp\])") },
        { "py",   QRegularExpression(R"(\[/py\])") },
        { "code", QRegularExpression(R"(\[/code\])") }
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
    if (prev != STATE_NONE) {
        struct CodeBlock {
            int stateFlag;
            QString styleKey;
            QRegularExpression closeRe;
        };

        const QVector<CodeBlock> blocks = {
            { STATE_CODE_CPP, "cpp", QRegularExpression(R"(\[/cpp\])") },
            { STATE_CODE,     "code", QRegularExpression(R"(\[/code\])") },
            { STATE_CPP,      "py", QRegularExpression(R"(\[/py\])") }
        };

        for (const auto& blk : blocks) {
            if (prev & blk.stateFlag) {
                QTextCharFormat fmt = styledTagsMap.value(blk.styleKey).format;
                QRegularExpressionMatch close = blk.closeRe.match(text);
                if (close.hasMatch()) {
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
                } else {
                    setFormat(0, text.length(), fmt);
                    currentBlockStateWithFlag(blk.stateFlag);
                    _codeRangesThisLine.append({ 0, text.length() });
                    return true; // nadal jesteśmy w wieloliniowym bloku
                }
            }
        }
    }

    while (offset < text.length()) {
        QRegularExpressionMatch openMatch = openRe.match(text, offset);
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

        if (tag == "cpp" || src.contains("cpp") || src.contains("c++")) {
            styleKey = "cpp";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CODE_CPP;
            closeRe = closeReMap["cpp"];
        } else if (tag == "py") {
            styleKey = "py";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CPP;
            closeRe = closeReMap["py"];
        } else {
            styleKey = "code";
            fmt = styledTagsMap.value(styleKey).format;
            stateFlag = STATE_CODE;
            closeRe = closeReMap["code"];
        }

        QRegularExpressionMatch closeMatch = closeRe.match(text, tagEnd);
        if (closeMatch.hasMatch()) {
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
        } else {
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

    static const QMap<QString, int> tagStates = {
                                                 { "b", STATE_STYLE_BOLD },
                                                 { "i", STATE_STYLE_ITALIC },
                                                 { "u", STATE_STYLE_UNDERLINE },
                                                 { "s", STATE_STYLE_STRIKE },
                                                 };

    static const QRegularExpression openRe(R"(\[(b|i|u|s)\])");
    static const QMap<QString, QRegularExpression> closeRes = {
                                                               { "b", QRegularExpression(R"(\[/b\])") },
                                                               { "i", QRegularExpression(R"(\[/i\])") },
                                                               { "u", QRegularExpression(R"(\[/u\])") },
                                                               { "s", QRegularExpression(R"(\[/s\])") },
                                                               };

    static QTextCharFormat tagFmt = [] {
        QTextCharFormat fmt;
        fmt.setForeground(Qt::gray);
        fmt.setFontPointSize(8);
        return fmt;
    }();

    // --- 1. Kontynuacja wieloliniowego stylu ---
    if (prev != STATE_NONE)
    {
        for (auto it = tagStates.constBegin(); it != tagStates.constEnd(); ++it) {
            const QString& tag = it.key();
            const int flag = it.value();

            if (prev & flag) {
                const QRegularExpression& closeRe = closeRes[tag];
                QTextCharFormat fmt = tagFormats[tag];

                QRegularExpressionMatch closeMatch = closeRe.match(text);
                if (closeMatch.hasMatch()) {
                    int closeStart = closeMatch.capturedStart();

                    // Jeśli zamknięcie stylu znajduje się wewnątrz kodu — pomiń cały styl
                    if (overlapsWithCode(0, closeStart + closeMatch.capturedLength()))
                        return false;
                    if (overlapsWithNoFormat(0, closeStart + closeMatch.capturedLength()))
                        return false;

                    setFormat(0, closeStart, fmt);
                    setFormat(closeStart, closeMatch.capturedLength(), tagFmt);
                    currentBlockStateWithoutFlag(flag);
                } else {
                    // Cała linia stylizowana — tylko jeśli nie pokrywa się z kodem
                    if (!overlapsWithCode(0, text.length()) && !overlapsWithNoFormat(0, text.length()))
                    {
                        setFormat(0, text.length(), fmt);
                        currentBlockStateWithFlag(flag);
                    }
                }
                return true;
            }
        }
    }

    // --- 2. Styl jednolinijkowy (może być wiele w jednej linii) ---
    int offset = 0;
    bool foundAny = false;

    while (offset < text.length()) {
        QRegularExpressionMatch match = openRe.match(text, offset);
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

            // IGNORUJ tagi stylizujące wewnątrz kodu
            if (overlapsWithCode(tagStart, closeEnd - tagStart) || overlapsWithNoFormat(tagStart, closeEnd - tagStart)) {
                offset = closeEnd;
                continue;
            }

            setFormat(tagStart, tagEnd - tagStart, tagFmt);           // [b]
            setFormatKeepingBackground(contentStart, contentLen, fmt);                 // treść
            setFormat(closeStart, closeEnd - closeStart, tagFmt);     // [/b]

            offset = closeEnd;
            foundAny = true;
        } else {
            // Początek wieloliniowego stylu — tylko jeśli nie zaczyna się w kodzie
            if (!overlapsWithCode(tagStart, text.length() - tagStart) && !overlapsWithNoFormat(tagStart, text.length() - tagStart)) {
                setFormat(tagStart, tagEnd - tagStart, tagFmt);
                setFormatKeepingBackground(tagEnd, text.length() - tagEnd, fmt);
                currentBlockStateWithFlag(flag);
                return true;
            } else {
                // Jeśli był w kodzie — pomiń
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
        QTextCharFormat baseFmt = this->format(i); // odzyskaj bieżący format
        baseFmt.setFontWeight(format.fontWeight());
        baseFmt.setFontItalic(format.fontItalic());
        baseFmt.setFontUnderline(format.fontUnderline());
        baseFmt.setFontStrikeOut(format.fontStrikeOut());
        setFormat(i, 1, baseFmt);
    }
}

bool STCSyntaxHighlighter::overlapsWithRange(int start, int length, const QVector<QPair<int, int>>& range)
{
    for (const auto& range : range) {
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
    // Reguła dla [a href="..." name="..."]
    static const QRegularExpression anchorRe(R"__(\[a\s+href="([^"]+)"\s+name="([^"]+)"\])__");

    // Reguła dla [img src="..." alt="..." opis="..."] – atrybuty alt i opis mogą być opcjonalne
    static const QRegularExpression imgRe(R"__(
    \[img\s+
    (?:
        (?:src="([^"]+)")|
        (?:alt="([^"]*)")|
        (?:opis="([^"]*)")|
        (?:autofit\b)
    )
    (?:\s+
        (?:
            (?:src="([^"]+)")|
            (?:alt="([^"]*)")|
            (?:opis="([^"]*)")|
            (?:autofit\b)
        )
    )*
    \s*\])__", QRegularExpression::ExtendedPatternSyntaxOption);

    bool found = false;

    // Szukaj linków
    auto anchorIt = anchorRe.globalMatch(text);
    while (anchorIt.hasNext()) {
        auto match = anchorIt.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        if (overlapsWithNoFormat(start, end - start))
            continue;

        // tag
        setFormat(start, end - start, styledTagsMap.value("tag.attr").format);

        // href
        int hrefStart = match.capturedStart(1);
        int hrefLen = match.capturedLength(1);
        setFormat(hrefStart, hrefLen, styledTagsMap.value("a.href").format);

        // name
        int nameStart = match.capturedStart(2);
        int nameLen = match.capturedLength(2);
        setFormat(nameStart, nameLen, styledTagsMap.value("a.name").format);

        found = true;
    }

    // Szukaj obrazków
    auto imgIt = imgRe.globalMatch(text);
    while (imgIt.hasNext()) {
        auto match = imgIt.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        if (overlapsWithNoFormat(start, end - start))
            continue;

        // Formatuj cały tag
        setFormat(start, end - start, styledTagsMap.value("tag.attr").format);

        // Znajdź wszystkie atrybuty w dopasowanym tekście
        QString matchedText = match.captured(0);

        // Szukaj src
        QRegularExpression srcRe(R"__(src="([^"]+)")__");
        auto srcMatch = srcRe.match(matchedText);
        if (srcMatch.hasMatch()) {
            int srcStart = start + srcMatch.capturedStart(1);
            int srcLen = srcMatch.capturedLength(1);
            setFormat(srcStart, srcLen, styledTagsMap.value("img.src").format);
        }

        // Szukaj alt
        QRegularExpression altRe(R"__(alt="([^"]+)")__");
        auto altMatch = altRe.match(matchedText);
        if (altMatch.hasMatch()) {
            int altStart = start + altMatch.capturedStart(1);
            int altLen = altMatch.capturedLength(1);
            setFormat(altStart, altLen, styledTagsMap.value("img.alt").format);
        }

        // Szukaj opis
        QRegularExpression opisRe(R"__(opis="([^"]+)")__");
        auto opisMatch = opisRe.match(matchedText);
        if (opisMatch.hasMatch()) {
            int opisStart = start + opisMatch.capturedStart(1);
            int opisLen = opisMatch.capturedLength(1);
            setFormat(opisStart, opisLen, styledTagsMap.value("img.opis").format);
        }

        // Szukaj autofit
        QRegularExpression autofitRe(R"__(autofit\b)__");
        auto autofitMatch = autofitRe.match(matchedText);
        if (autofitMatch.hasMatch())
        {
            int autofitStart = start + autofitMatch.capturedStart(0);
            int autofitLen = autofitMatch.capturedLength(0);
            setFormat(autofitStart, autofitLen, styledTagsMap.value("img.autofit").format);
        }

        found = true;
    }

    return found;
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
