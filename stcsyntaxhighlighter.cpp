#include "stcsyntaxhighlighter.h"
#include <QRegularExpression>

#warning "Chat helped me with the code, but it requires refactoring"
// TODO: Chat helped me with the code, but it requires refactoring
STCSyntaxHighlighter::STCSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Styl dla samych tagów: [b], [/b] — odpowiada klasie .tag w CSS
    QTextCharFormat tagFormat;
    tagFormat.setForeground(Qt::gray);
    tagFormat.setFontPointSize(8);
    rules.append({ QRegularExpression(R"(\[/?\w+(=[^\]]+)?\])"), tagFormat });

    // Dodanie stylów zgodnych z CSS
    addBlockStyle("h1", QColor("#a33"), true, 20);
    addBlockStyle("h2", QColor("#a33"), true, 17);
    addBlockStyle("h3", QColor("#a33"), false, 14);
    addBlockStyle("h4", QColor("#a33"), false, 11);

    addBlockStyle("tip", Qt::white, false, -1, QColor("darkgreen"));
    addBlockStyle("warning", Qt::white, false, -1, QColor("red"));
    addBlockStyle("cytat", Qt::black, false, -1, QColor("orange"));

    addBlockStyle("code", QColor("yellow"), false, -1, QColor("black"), "monospace");
    addBlockStyle("cpp", QColor("black"), false, -1, QColor("lightblue"), "monospace");
    addBlockStyle("py", QColor("black"), false, -1, QColor("brown"), "monospace");

    addBlockStyle("b", QColor::Invalid, true);
    addBlockStyle("href", QColor("lightblue"), false);
}

void STCSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 1. Stylizacja pełnych tagów z zawartością, np. [b]tekst[/b]
    for (const auto &styled : styledTags) {
        QRegularExpression re(QString(R"(\[(%1)(=[^\]]*)?\](.*?)\[/\1\])").arg(styled.tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();

            int fullStart = match.capturedStart(0);
            int fullLen = match.capturedLength(0);

            int tagOpenStart = match.capturedStart(0);
            int tagOpenLen = match.capturedStart(3) - tagOpenStart;

            int contentStart = match.capturedStart(3);
            int contentLen = match.capturedLength(3);

            int tagCloseStart = contentStart + contentLen;
            int tagCloseLen = (fullStart + fullLen) - tagCloseStart;

            // Styl tagów
            QTextCharFormat tagFormat;
            tagFormat.setForeground(Qt::gray);
            tagFormat.setFontPointSize(8);

            setFormat(tagOpenStart, tagOpenLen, tagFormat);
            setFormat(tagCloseStart, tagCloseLen, tagFormat);

            // Styl środka
            setFormat(contentStart, contentLen, styled.format);
        }
    }

    // 2. Stylizacja tagów z atrybutami, np. [a href="..." name="..."]
    QRegularExpression tagWithAttrs(R"(\[(\w+)((?:\s+\w+(="[^"]*")?)*)\s*\])");
    QRegularExpressionMatchIterator it = tagWithAttrs.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        int tagStart = match.capturedStart(0);
        int tagLen = match.capturedLength(0);
        int attrStart = match.capturedStart(2);
        QString attrText = match.captured(2);

        // Styl bazowy dla tagu
        QTextCharFormat tagFormat;
        tagFormat.setForeground(Qt::gray);
        tagFormat.setFontPointSize(8);
        setFormat(tagStart, tagLen, tagFormat); // domyślnie wszystko szare

        // Dopasowanie poszczególnych atrybutów
        QRegularExpression attrRe(R"attr((\w+)(="([^"]*)")?)attr");
        QRegularExpressionMatchIterator attrIt = attrRe.globalMatch(attrText);

        while (attrIt.hasNext()) {
            QRegularExpressionMatch amatch = attrIt.next();

            int keyStart = attrStart + amatch.capturedStart(1);
            int keyLen = amatch.capturedLength(1);

            int eqQuoteStart = attrStart + amatch.capturedStart(2);
            int eqQuoteLen = amatch.capturedLength(2);

            int valStart = attrStart + amatch.capturedStart(3);
            int valLen = amatch.capturedLength(3);

            // 2.1: Nazwa atrybutu i =" — szare, jak tag
            if (eqQuoteLen > 0)
                setFormat(keyStart, eqQuoteStart + eqQuoteLen - keyStart, tagFormat);
            else
                setFormat(keyStart, keyLen, tagFormat); // np. [pkt ext]

            // 2.2: Wartość — niebieska
            if (valLen > 0) {
                QTextCharFormat valFormat;
                valFormat.setForeground(QColor("#0033cc")); // linkowy niebieski
                setFormat(valStart, valLen, valFormat);
            }
        }
    }

    // 3. Stylizacja samych tagów (fallback – np. [b], [/b], bez parowania)
    for (const auto &rule : rules) {
        QRegularExpressionMatchIterator ruleIt = rule.pattern.globalMatch(text);
        while (ruleIt.hasNext()) {
            auto match = ruleIt.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

void STCSyntaxHighlighter::addBlockStyle(const QString &tag,
                                         QColor foreground,
                                         bool bold,
                                         int pointSize,
                                         QColor background,
                                         const QString &fontFamily)
{
    QTextCharFormat fmt;
    if (foreground.isValid())
        fmt.setForeground(foreground);
    if (background.isValid())
        fmt.setBackground(background);
    if (bold)
        fmt.setFontWeight(QFont::Bold);
    if (pointSize > 0)
        fmt.setFontPointSize(pointSize);
    if (!fontFamily.isEmpty())
        fmt.setFontFamilies({fontFamily});

    styledTags.append({ tag, fmt });
}
