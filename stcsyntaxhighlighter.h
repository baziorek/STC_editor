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
    };

    QVector<Rule> rules;
    QVector<StyledTag> styledTags;

    void addBlockStyle(const QString &tag,
                       QColor foreground = Qt::black,
                       bool bold = false,
                       int pointSize = -1,
                       QColor background = QColor(),
                       const QString &fontFamily = QString());
};
