#include "stcsyntaxhighlighter.h"
#include <QRegularExpression>
#include "types/stcTags.h"


STCSyntaxHighlighter::STCSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    using enum StcTags;

    QTextCharFormat tagFormat;
    tagFormat.setForeground(Qt::gray);
    tagFormat.setFontPointSize(8);
    rules.append({ QRegularExpression(R"(\[/?\w+(=[^\]]+)?\])"), tagFormat });

    addBlockStyle(tagsClasses[H1], QColor("#a33"), std::to_underlying(BOLD), 20);
    addBlockStyle(tagsClasses[H2], QColor("#a33"), std::to_underlying(BOLD), 17);
    addBlockStyle(tagsClasses[H3], QColor("#a33"), std::to_underlying(NONE), 14);
    addBlockStyle(tagsClasses[H4], QColor("#a33"), std::to_underlying(NONE), 11);

    addBlockStyle("tip", Qt::white, std::to_underlying(NONE), -1, QColor("darkgreen"));
    addBlockStyle("warning", Qt::white, std::to_underlying(NONE), -1, QColor("red"));
    addBlockStyle(tagsClasses[QUOTE], Qt::black, std::to_underlying(NONE), -1, QColor("orange"));

    addBlockStyle(tagsClasses[CODE], QColor("yellow"), std::to_underlying(NONE), -1, QColor("black"), "monospace");
    addBlockStyle(tagsClasses[CPP], QColor("black"), std::to_underlying(NONE), -1, QColor("lightblue"), "monospace");
    addBlockStyle(tagsClasses[PY], QColor("black"), std::to_underlying(NONE), -1, QColor("brown"), "monospace");

    addBlockStyle(tagsClasses[BOLD], QColor::Invalid, std::to_underlying(BOLD));
    addBlockStyle(tagsClasses[ITALIC], QColor::Invalid, std::to_underlying(ITALIC));
    addBlockStyle(tagsClasses[UNDERLINED], QColor::Invalid, std::to_underlying(UNDERLINED));
    addBlockStyle(tagsClasses[STRUCK_OUT], QColor::Invalid, std::to_underlying(STRUCK_OUT));
    addBlockStyle("href", QColor("lightblue"), std::to_underlying(NONE));
}

#warning "Chat helped me with the code, but it requires refactoring and corrections"
void STCSyntaxHighlighter::highlightBlock(const QString &text)
{
    // Multilinijkowy kod: [cpp]...[/cpp], [code]...[/code], [py]...[/py]
    static const QMap<QString, QTextCharFormat> codeBlockFormats = {
        { "cpp", styledTagsMap.value("cpp").format },
        { "code", styledTagsMap.value("code").format },
        { "py", styledTagsMap.value("py").format }
    };

    // 1. Czy aktualny blok rozpoczyna się od znacznika?
    QRegularExpression openTagRe(QStringLiteral("^\\[(cpp|code|py)\\]"));
    QRegularExpression closeTagRe(QStringLiteral("\\[/\\w+\\]"));

    // Jeśli jesteśmy w środku bloku kodu (ustawionym wcześniej)
    QString blockType;
    if (previousBlockState() > 0) {
        blockType = styledTags[previousBlockState() - 1].tag;
    }

    bool blockContinues = false;
    if (!blockType.isEmpty()) {
        // Stylizuj cały blok jako kod
        auto fmt = codeBlockFormats.value(blockType);
        setFormat(0, text.length(), fmt);

        // Czy kończy się tu blok?
        if (text.contains(QRegularExpression(QStringLiteral("\\[/%1\\]").arg(blockType)))) {
            setCurrentBlockState(-1);  // reset
        } else {
            setCurrentBlockState(previousBlockState());
            return;  // nie przetwarzamy dalej
        }
    } else {
        // Czy to otwierający tag?
        QRegularExpressionMatch match = openTagRe.match(text);
        if (match.hasMatch()) {
            blockType = match.captured(1);
            if (styledTagsMap.contains(blockType)) {
                int tagIndex = styledTags.indexOf(styledTagsMap.value(blockType));
                setCurrentBlockState(tagIndex + 1);

                // NOWOŚĆ: kolorujemy zawartość za tagiem
                int tagEnd = match.capturedEnd(0);
                QTextCharFormat fmt = styledTagsMap.value(blockType).format;
                setFormat(tagEnd, text.length() - tagEnd, fmt);
            }
        }
    }

    QVector<QPair<int, int>> protectedRanges;

    // 1. Stylizacja div[class=tip/uwaga]
    QRegularExpression divRe(QStringLiteral("\\[div\\s+class=\"(tip|uwaga)\"\\](.*?)\\[/div\\]"));
    QRegularExpressionMatchIterator divIt = divRe.globalMatch(text);

    while (divIt.hasNext()) {
        QRegularExpressionMatch match = divIt.next();
        QString divClass = match.captured(1);
        int fullStart = match.capturedStart(0);
        int fullLen = match.capturedLength(0);
        int contentStart = match.capturedStart(2);
        int contentLen = match.capturedLength(2);

        QTextCharFormat tagFmt;
        tagFmt.setForeground(Qt::gray);
        tagFmt.setFontPointSize(8);
        setFormat(fullStart, contentStart - fullStart, tagFmt);  // tag otwierający
        setFormat(contentStart + contentLen, fullStart + fullLen - (contentStart + contentLen), tagFmt);  // zamykający

        QTextCharFormat bgFmt;
        bgFmt.setFontItalic(true);
        if (divClass == "tip")
            bgFmt.setBackground(QColor("#229922"));
        else if (divClass == "uwaga")
            bgFmt.setBackground(QColor("#ff7777"));

        setFormat(contentStart, contentLen, bgFmt);

        // Zapamiętaj zakres, żeby nie nadpisywać tła
        protectedRanges.append({ contentStart, contentLen });
    }

    // 2. Stylizacja tagów z atrybutami np. [a href="..."]
    QRegularExpression tagWithAttrs(QStringLiteral("\\[(\\w+)((?:\\s+\\w+(=\"[^\"]*\")?)*)\\s*\\]"));
    QRegularExpressionMatchIterator it = tagWithAttrs.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        int tagStart = match.capturedStart(0);
        int tagLen = match.capturedLength(0);
        int attrStart = match.capturedStart(2);
        QString attrText = match.captured(2);

        QTextCharFormat tagFormat;
        tagFormat.setForeground(Qt::gray);
        tagFormat.setFontPointSize(8);
        setFormat(tagStart, tagLen, tagFormat);

        QRegularExpression attrRe(QStringLiteral("(\\w+)(=\"([^\"]*)\")?"));
        QRegularExpressionMatchIterator attrIt = attrRe.globalMatch(attrText);

        while (attrIt.hasNext()) {
            QRegularExpressionMatch amatch = attrIt.next();

            int keyStart = attrStart + amatch.capturedStart(1);
            int keyLen = amatch.capturedLength(1);

            int eqQuoteStart = attrStart + amatch.capturedStart(2);
            int eqQuoteLen = amatch.capturedLength(2);

            int valStart = attrStart + amatch.capturedStart(3);
            int valLen = amatch.capturedLength(3);

            if (eqQuoteLen > 0)
                setFormat(keyStart, eqQuoteStart + eqQuoteLen - keyStart, tagFormat);
            else
                setFormat(keyStart, keyLen, tagFormat);

            if (valLen > 0) {
                QTextCharFormat valFormat;
                valFormat.setForeground(QColor("#0033cc"));
                setFormat(valStart, valLen, valFormat);
            }
        }
    }

    // 3. Stylizacja pełnych tagów z zawartością, np. [b]tekst[/b]
    for (const auto &styled : styledTags) {
        QRegularExpression re(QStringLiteral("\\[(%1)(=[^\\]]*)?\\](.*?)\\[/\\1\\]").arg(styled.tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int fullStart = match.capturedStart(0);
            int fullLen = match.capturedLength(0);
            int contentStart = match.capturedStart(3);
            int contentLen = match.capturedLength(3);
            int tagOpenStart = match.capturedStart(0);
            int tagOpenLen = contentStart - tagOpenStart;
            int tagCloseStart = contentStart + contentLen;
            int tagCloseLen = fullStart + fullLen - tagCloseStart;

            // Tag otwierający i zamykający — małe, szare
            QTextCharFormat tagFmt;
            tagFmt.setForeground(Qt::gray);
            tagFmt.setFontPointSize(8);
            setFormat(tagOpenStart, tagOpenLen, tagFmt);
            setFormat(tagCloseStart, tagCloseLen, tagFmt);

            // Treść — z zachowaniem tła (jeśli już jest)
            QTextCharFormat finalFmt = styled.format;
            QTextCharFormat current = format(contentStart);
            if (current.hasProperty(QTextFormat::BackgroundBrush))
                finalFmt.setBackground(current.background());

            setFormat(contentStart, contentLen, finalFmt);
        }
    }

    // 4. Fallback: stylizacja luźnych tagów, o ile to nie kod C++
    if (!text.contains(QRegularExpression(R"(\b(int|char|return|#include|->|::|new|delete|using)\b)"))) {
        for (const auto &rule : rules) {
            QRegularExpressionMatchIterator ruleIt = rule.pattern.globalMatch(text);
            while (ruleIt.hasNext()) {
                auto match = ruleIt.next();
                int start = match.capturedStart();
                int len = match.capturedLength();

                // Pomijamy jeśli w protected zakresie
                bool skip = false;
                for (const auto& r : protectedRanges) {
                    if (start >= r.first && start < r.first + r.second) {
                        skip = true;
                        break;
                    }
                }

                if (!skip)
                    setFormat(start, len, rule.format);
            }
        }
    }
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
