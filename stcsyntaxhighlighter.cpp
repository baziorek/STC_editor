#include "stcsyntaxhighlighter.h"
#include <QRegularExpression>

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
    // Najpierw: pokoloruj same tagi (np. [b], [/b])
    for (const auto &rule : rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Następnie: pokoloruj zawartość tagów
    for (const auto &styled : styledTags) {
        // Matchuje np. [b]tekst[/b], [tip]abc[/tip]
        QRegularExpression re(QString(R"(\[(%1)(=[^\]]*)?\](.*?)\[/\1\])").arg(styled.tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();

            // Pozycje i długości
            int fullStart = match.capturedStart(0);
            int fullLen = match.capturedLength(0);

            int tagOpenStart = match.capturedStart(0);
            int tagOpenLen = match.capturedStart(3) - tagOpenStart;

            int contentStart = match.capturedStart(3);
            int contentLen = match.capturedLength(3);

            int tagCloseStart = contentStart + contentLen;
            int tagCloseLen = (fullStart + fullLen) - tagCloseStart;

            // 1. Styl tagów [b], [/b]
            QTextCharFormat tagFormat;
            tagFormat.setForeground(Qt::gray);
            tagFormat.setFontPointSize(8);

            setFormat(tagOpenStart, tagOpenLen, tagFormat);
            setFormat(tagCloseStart, tagCloseLen, tagFormat);

            // 2. Styl środka
            setFormat(contentStart, contentLen, styled.format);
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
