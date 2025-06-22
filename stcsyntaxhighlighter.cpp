#include <QRegularExpression>
#include "stcsyntaxhighlighter.h"

STCSyntaxHighlighter::STCSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Formatowanie dla tagów – jak .tag
    QTextCharFormat tagFormat;
    tagFormat.setForeground(Qt::gray);
    tagFormat.setFontPointSize(8);
    rules.append({ QRegularExpression("\\[/?\\w+(=[^\\]]+)?\\]"), tagFormat });

    // --- dodajemy blokowe tagi z formatowaniem z CSS ---

    // h1
    addBlockStyle("h1", QColor("#a33"), true, 20);
    addBlockStyle("h2", QColor("#a33"), true, 17);
    addBlockStyle("h3", QColor("#a33"), false, 14);
    addBlockStyle("h4", QColor("#a33"), false, 11);

    // tip
    addBlockStyle("tip", Qt::white, false, -1, QColor("darkgreen"));

    // warning
    addBlockStyle("warning", Qt::white, false, -1, QColor("red"));

    // cytat
    addBlockStyle("cytat", Qt::black, false, -1, QColor("orange"));

    // pkt – nie ma stylu w CSS, można pominąć lub nadać własny

    // code
    addBlockStyle("code", QColor("yellow"), false, -1, QColor("black"), "monospace");

    // cpp
    addBlockStyle("cpp", QColor("black"), false, -1, QColor("lightblue"), "monospace");

    // py
    addBlockStyle("py", QColor("black"), false, -1, QColor("brown"), "monospace");

    // a (href)
    addBlockStyle("href", QColor("lightblue"), false);
}

void STCSyntaxHighlighter::highlightBlock(const QString &text)
{
    // podświetlenie samych tagów
    for (const auto &rule : rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // podświetlenie zawartości całych bloków
    for (const auto &styled : styledTags) {
        QRegularExpression re(QString(R"(\[%1(?:=[^\]]*)?\](.*?)\[/%1\])").arg(styled.tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            int start = match.capturedStart();
            int length = match.capturedLength();
            setFormat(start, length, styled.format);
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
    if (foreground.isValid()) fmt.setForeground(foreground);
    if (background.isValid()) fmt.setBackground(background);
    if (bold) fmt.setFontWeight(QFont::Bold);
    if (pointSize > 0) fmt.setFontPointSize(pointSize);
    if (!fontFamily.isEmpty()) fmt.setFontFamily(fontFamily);

    styledTags.append({ tag, fmt });
}
