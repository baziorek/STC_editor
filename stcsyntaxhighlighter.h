#pragma once

#include <source_location>
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

    bool highlightHeading(const QString &text);
    bool highlightDivBlock(const QString &text);
    bool highlightPktOrCsv(const QString &text);
    bool highlightCodeBlock(const QString &text);
    bool highlightTextStyleTags(const QString& text);
    bool highlightTagsWithAttributes(const QString& text);

    void currentBlockStateWithoutFlag(int flag, std::source_location location=std::source_location::current());
    void currentBlockStateWithFlag(int flag, std::source_location location=std::source_location::current());

    bool overlapsWithCode(int start, int length) const;

    void setFormatKeepingBackground(int contentStart, int contentLen, const QTextCharFormat &format);

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

    QVector<QPair<int, int>> _codeRangesThisLine; // position start and length
};
