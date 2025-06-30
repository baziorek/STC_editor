#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <QString>
#include <QRegularExpression>

/// class inspired with: https://doc.qt.io/qt-6.2/qtwidgets-richtext-syntaxhighlighter-example.html
class STCSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    STCSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

    void addBlockStyle(const QString &tag,
                       QColor foreground = Qt::black,
                       std::uint64_t format = {},
                       int pointSize = -1,
                       QColor background = QColor(),
                       const QString &fontFamily = QString());

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    struct StyledTag
    {
        QString tag;
        QTextCharFormat format;

        bool operator==(const StyledTag &other) const
        {
            return tag == other.tag;
        }
    };

    QVector<Rule> rules;
    QVector<StyledTag> styledTags;

    QMap<QString, StyledTag> styledTagsMap;
};

/*
 * Działanie trzeba zmienić (kolejność):
1. Wpierw niech będą wykrywane sekcje tekstu, czyli:
`[div]tekst[/div]`, ale też `[cytat]tekst[/cytat]`, a także bardziej skomplikowane:
`[div class="tip"]tekst[/div]` i `[div class="uwaga"]tekst[/div]`
Każdy z tych może być w jednej linii, ale też może być w środku (nie od początku i nie do końca linii). Niech zmieniają tło (i w sumie tylko tło).
Oprócz tego jeśli mamy tego diva z klasą `[div class="tip"]tekst[/div]`  to niech tekst "tip" będzie bardziej zaznaczony (w sumie to jest).
2. Następnie niech będą wykrywane kody, czyli:
`[cpp]tekst[/cpp]`, podobnie dla `[code]` i `[py]` - wewnątrz kodów tylko formatowanie kodu, żadnych więcej reguł. Jednakże kod może być w jednej linii, jak i wielolinijkowy. Poza tym sekcja otwierająca i zamykająca kod może być w środku linii np. `[run][cpp]#include <iostream>[/cpp][/run]`
3. Następnie formatowania nagłówków: `[h1]Nagłówek[/h1]`, nagłówki są bodajże 1-6. Wewnątrz nagłówków mogą być linki i pogrubienia
4. Formatowanie w formie pogrubienia `[b]pogrubiony[/b]` i inne https://cpp0x.pl/kursy/Kurs-STC/Podstawy/Znaczniki-z-HTML-039-a/170
*/
