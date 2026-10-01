#pragma once

#include <source_location>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <QString>
#include <QRegularExpression>
#include "utils/SpellChecker.h"
#include "utils/SyntaxMode.h"


/// class inspired with: https://doc.qt.io/qt-6.2/qtwidgets-richtext-syntaxhighlighter-example.html
class STCSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    STCSyntaxHighlighter(QTextDocument *parent = nullptr);

    const SpellChecker& getSpellChecker() const
    {
        return spellChecker;
    }

    /// `Stc` highlights the STC markup (and the code inside of `[cpp]`, `[py]`, ... blocks),
    /// the other modes highlight the entire document as a source file in that language.
    SyntaxMode syntaxMode() const
    {
        return _mode;
    }
    /// Pass `rehighlightNow=false` when the document content is about to be replaced anyway.
    void setSyntaxMode(SyntaxMode mode, bool rehighlightNow = true);

protected:
    void highlightBlock(const QString &text) override;
    bool highlightHeading(const QString &text);
    bool highlightDivBlock(const QString &text);
    bool highlightPktOrCsv(const QString &text);
    bool highlightCodeBlock(const QString &text);
    bool highlightTextStyleTags(const QString& text);
    bool highlightTagsWithAttributes(const QString& text);
    void highlightPlainTextContent(const QString &text);

    /// Highlighting of the code in `[0, to)`; it is the state of the previous line what says whether
    /// a construct (`/* ... */`, `"""..."""`) is continued. All of them return the bits of the state which
    /// the next line has to know about (they are merged to the block state once per line, see `highlightBlock`).
    int applyCppHighlighting(const QString &text, int from, int to, int stateIn);
    int applyPythonHighlighting(const QString &text, int from, int to, int stateIn);
    int applyCodeHighlighting(int codeBlockStateFlag, const QString &text, int from, int to, int stateIn);

    /// Used when the syntax mode is not `Stc`: the whole block is a line of a source file.
    void highlightSourceFileBlock(const QString &text);
    int languageStateFromPreviousBlock() const;
    void mergeLanguageStateIntoBlockState();

    void addBlockStyle(const QString &tag,
                       QColor foreground = Qt::black,
                       std::uint64_t format = {},
                       int pointSize = -1,
                       QColor background = QColor(),
                       const QString &fontFamily = QString());

    void currentBlockStateWithoutFlag(int flag, std::source_location location=std::source_location::current());
    void currentBlockStateWithFlag(int flag, std::source_location location=std::source_location::current());

    bool overlapsWithCode(int start, int length) const
    {
        return overlapsWithRange(start, length, _codeRangesThisLine);
    }

    bool overlapsWithNoFormat(int start, int length) const
    {
        return overlapsWithRange(start, length, _noFormatRangesThisLine);
    }

    static bool overlapsWithRange(int start, int length, const QVector<QPair<int, int>>& range);

    void setFormatKeepingBackground(int contentStart, int contentLen, const QTextCharFormat &format);

    void applySpellcheckToTextRange(const QString &text, int start, int length, const QTextCharFormat &baseFormat);

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

    QVector<QPair<int, int>> _codeRangesThisLine;     // position start and length
    QVector<QPair<int, int>> _noFormatRangesThisLine; // position start and length

    SpellChecker spellChecker;

    SyntaxMode _mode = SyntaxMode::Stc;
    int _languageStateBits = 0; // what the code highlighting of this line wants the next line to know
};
