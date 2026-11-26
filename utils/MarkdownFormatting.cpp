#include "MarkdownFormatting.h"

#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace
{

/// The selected text without the whitespace at its edges; `leading`/`trailing` get the widths
/// of the removed affixes so they can be put back around the result.
QString trimmedSelection(const QString& selected, int& leading, int& trailing)
{
    const QString trimmed = selected.trimmed();
    leading = selected.indexOf(trimmed);
    trailing = selected.length() - trimmed.length() - leading;
    return trimmed;
}

/// Whether `text` is wrapped in `marker` on both sides. For the single-character markers `*` and
/// `` ` `` a doubled marker (`**`, `` `` ``) does not count, so that e.g. bold text is not
/// mistaken for italic.
bool isWrappedBy(const QString& text, const QString& marker)
{
    if (text.size() < marker.size() * 2)
        return false;
    if (!text.startsWith(marker) || !text.endsWith(marker))
        return false;
    if (marker.size() == 1 && text.size() > marker.size() * 2)
    {
        // "*x*" is italic, "**x**" is bold: for "*" the neighbour of the edge marker must differ
        if (text.at(1) == marker.at(0) || text.at(text.size() - 2) == marker.at(0))
            return false;
    }
    return true;
}

/// The text between the markers right before `start` and right after `end` in `document`,
/// or false when the positions do not hold exactly `marker`.
bool markersAround(QTextDocument* document, int start, int end, const QString& marker)
{
    if (start - marker.size() < 0 || end + marker.size() > document->characterCount() - 1)
        return false;

    const auto textAt = [document](int pos, int len) {
        QTextCursor c(document);
        c.setPosition(pos);
        c.setPosition(pos + len, QTextCursor::KeepAnchor);
        return c.selectedText();
    };

    if (textAt(start - marker.size(), marker.size()) != marker
        || textAt(end, marker.size()) != marker)
        return false;

    // a single "*" or "`" must not be part of a longer run: the "*"s around "bold" in "**bold**"
    // are bold markers, not a pair of italic ones
    if (marker.size() == 1)
    {
        const QChar m = marker.at(0);
        if (start - marker.size() - 1 >= 0 && textAt(start - marker.size() - 1, 1) == m)
            return false;
        if (end + marker.size() < document->characterCount() - 1
            && textAt(end + marker.size(), 1) == m)
            return false;
    }
    return true;
}

void setCursorBetweenMarkers(QTextCursor& cursor, int markerSize)
{
    cursor.setPosition(cursor.position() - markerSize);
}

/// First and last text block touched by the selection (the whole current line without a
/// selection).
std::pair<int, int> selectedLineRange(const QTextCursor& cursor)
{
    const int selStart = cursor.selectionStart();
    const int selEnd = qMax(cursor.selectionStart(), cursor.selectionEnd() - 1);
    const QTextDocument* document = cursor.document();
    return { document->findBlock(selStart).blockNumber(),
             document->findBlock(selEnd).blockNumber() };
}

/// Replaces the text of one block, keeping its line break.
void replaceBlockText(QTextDocument* document, int blockNumber, const QString& newText)
{
    const QTextBlock block = document->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    QTextCursor cursor(document);
    cursor.setPosition(block.position());
    cursor.setPosition(block.position() + block.text().length(), QTextCursor::KeepAnchor);
    cursor.insertText(newText);
}

QStringList blockTexts(const QTextDocument* document, int firstBlock, int lastBlock)
{
    QStringList lines;
    for (int bn = firstBlock; bn <= lastBlock; ++bn)
    {
        const QTextBlock block = document->findBlockByNumber(bn);
        if (block.isValid())
            lines.append(block.text());
    }
    return lines;
}

/// First `level` hashes followed by a space/tab or the end of the line (the same shape the
/// highlighter accepts as an ATX heading).
const QRegularExpression& headingPrefixRe()
{
    // a custom delimiter: the pattern itself contains ")" characters
    static const QRegularExpression re(QStringLiteral(R"re(^( {0,3})(#{1,6})(?:[ \t]+|$))re"));
    return re;
}

/// The `> ` quote marker at the start of a line (up to three spaces of indentation, like the
/// highlighter allows).
const QRegularExpression& quotePrefixRe()
{
    static const QRegularExpression re(QStringLiteral(R"re(^( {0,3})>[ \t]?)re"));
    return re;
}

std::pair<QString, QString> extractMarkdownLink(const QString& text)
{
    static const QRegularExpression regex(QStringLiteral(R"(https?://\S+|www\.\S+)"));
    const QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch())
        return {{}, text};

    QString description = (text.left(match.capturedStart()) + QLatin1Char(' ')
                           + text.mid(match.capturedEnd())).simplified();
    if (description == match.captured(0))
        description.clear();
    return { match.captured(0), description };
}

bool looksLikeImageSource(const QString& text)
{
    static const QRegularExpression extRe(
        QStringLiteral(R"(\.(?:png|jpe?g|gif|bmp|webp|svg|avif|tiff?)$)"),
        QRegularExpression::CaseInsensitiveOption);
    return extRe.match(text).hasMatch()
        || text.contains(QLatin1String("://"))
        || text.contains(QLatin1Char('/'));
}

} // namespace

void markdownedit::toggleInline(QPlainTextEdit* editor, const QString& marker)
{
    QTextCursor cursor = editor->textCursor();
    const QString selected = cursor.selectedText();
    int leading = 0;
    int trailing = 0;
    const QString trimmed = trimmedSelection(selected, leading, trailing);

    // 1. the selection already carries the markers -> remove them
    if (isWrappedBy(trimmed, marker))
    {
        const QString unwrapped = trimmed.mid(marker.size(),
                                              trimmed.size() - marker.size() * 2);
        cursor.insertText(QString(selected.left(leading)) + unwrapped
                          + QString(selected.right(trailing)));
        return;
    }

    // 2. the selection sits between a pair of markers -> remove the markers
    if (!selected.isEmpty()
        && markersAround(cursor.document(), cursor.selectionStart(), cursor.selectionEnd(), marker))
    {
        const int selStart = cursor.selectionStart();
        const int selEnd = cursor.selectionEnd();

        QTextCursor tmp(cursor.document());
        tmp.setPosition(selEnd);
        tmp.setPosition(selEnd + marker.size(), QTextCursor::KeepAnchor);
        tmp.removeSelectedText();
        tmp.setPosition(selStart - marker.size());
        tmp.setPosition(selStart, QTextCursor::KeepAnchor);
        tmp.removeSelectedText();

        QTextCursor restore(cursor.document());
        restore.setPosition(selStart - marker.size());
        restore.setPosition(selStart - marker.size() + selected.size(), QTextCursor::KeepAnchor);
        editor->setTextCursor(restore);
        return;
    }

    // 3. wrap the selection; without a selection insert an empty pair
    if (selected.isEmpty())
    {
        cursor.insertText(marker + marker);
        setCursorBetweenMarkers(cursor, marker.size());
        editor->setTextCursor(cursor);
    }
    else
    {
        cursor.insertText(QString(selected.left(leading)) + marker + trimmed + marker
                          + QString(selected.right(trailing)));
    }
    editor->setFocus();
}

void markdownedit::toggleHeading(QPlainTextEdit* editor, int level)
{
    QTextCursor cursor = editor->textCursor();
    const auto [firstBlock, lastBlock] = selectedLineRange(cursor);
    const QString hashes = QString(QLatin1Char('#')).repeated(level);

    // bottom-up so the earlier line numbers stay valid while the text changes
    for (int bn = lastBlock; bn >= firstBlock; --bn)
    {
        const QTextBlock block = cursor.document()->findBlockByNumber(bn);
        if (!block.isValid())
            continue;

        const QString text = block.text();
        const auto match = headingPrefixRe().match(text);
        const QString core = match.hasMatch() ? text.mid(match.capturedLength()) : text;
        const int existingLevel = match.hasMatch() ? match.captured(2).size() : 0;

        QString newText;
        if (existingLevel == level)
            newText = core; // already a heading of this level -> plain text
        else
            newText = hashes + QLatin1Char(' ') + core;

        if (newText != text)
            replaceBlockText(cursor.document(), bn, newText);
    }
    editor->setFocus();
}

void markdownedit::toggleQuote(QPlainTextEdit* editor)
{
    QTextCursor cursor = editor->textCursor();
    const auto [firstBlock, lastBlock] = selectedLineRange(cursor);

    for (int bn = lastBlock; bn >= firstBlock; --bn)
    {
        const QTextBlock block = cursor.document()->findBlockByNumber(bn);
        if (!block.isValid())
            continue;

        const QString text = block.text();
        const auto match = quotePrefixRe().match(text);
        QString newText;
        if (match.hasMatch())
            newText = match.captured(1) + text.mid(match.capturedLength()); // drop one "> "
        else
            newText = QStringLiteral("> ") + text;

        if (newText != text)
            replaceBlockText(cursor.document(), bn, newText);
    }
    editor->setFocus();
}

void markdownedit::insertLink(QPlainTextEdit* editor)
{
    QTextCursor cursor = editor->textCursor();
    const QString selected = cursor.selectedText();

    if (selected.isEmpty())
    {
        cursor.insertText(QStringLiteral("[]()"));
        cursor.setPosition(cursor.position() - 1); // inside the parentheses
        editor->setTextCursor(cursor);
        editor->setFocus();
        return;
    }

    const auto [url, description] = extractMarkdownLink(selected);
    QString result;
    if (!url.isEmpty())
    {
        const QString text = description.isEmpty() ? url : description;
        result = QStringLiteral("[") + text + QStringLiteral("](") + url + QLatin1Char(')');
    }
    else
    {
        result = QStringLiteral("[") + selected + QStringLiteral("]()");
    }

    cursor.insertText(result);
    if (url.isEmpty())
        cursor.setPosition(cursor.position() - 1); // inside the parentheses
    editor->setTextCursor(cursor);
    editor->setFocus();
}

void markdownedit::insertImage(QPlainTextEdit* editor)
{
    QTextCursor cursor = editor->textCursor();
    const QString selected = cursor.selectedText().trimmed();

    QString result;
    if (selected.isEmpty())
    {
        result = QStringLiteral("![]()");
    }
    else if (looksLikeImageSource(selected))
    {
        result = QStringLiteral("![](") + selected + QLatin1Char(')');
    }
    else
    {
        result = QStringLiteral("![") + selected + QStringLiteral("]()");
    }

    cursor.insertText(result);
    cursor.setPosition(cursor.position() - 1); // inside the parentheses
    editor->setTextCursor(cursor);
    editor->setFocus();
}

void markdownedit::insertCodeBlock(QPlainTextEdit* editor, const QString& language)
{
    QTextCursor cursor = editor->textCursor();
    const QString openFence = QStringLiteral("```") + language;
    const QString closeFence = QStringLiteral("```");

    if (!cursor.hasSelection())
    {
        // the fences must sit on their own lines
        const QTextBlock block = cursor.block();
        const int pos = cursor.position();
        const bool atLineStart = pos == block.position();
        const bool atLineEnd = pos == block.position() + block.text().length();
        const QString prefix = atLineStart ? QString() : QStringLiteral("\n");
        const QString suffix = atLineEnd ? QString() : QStringLiteral("\n");

        cursor.insertText(prefix + openFence + QStringLiteral("\n\n") + closeFence + suffix);
        cursor.setPosition(pos + prefix.size() + openFence.size() + 1); // the empty line inside
        editor->setTextCursor(cursor);
        editor->setFocus();
        return;
    }

    const auto [firstBlock, lastBlock] = selectedLineRange(cursor);
    const QStringList lines = blockTexts(cursor.document(), firstBlock, lastBlock);
    if (lines.isEmpty())
        return;

    const QTextBlock first = cursor.document()->findBlockByNumber(firstBlock);
    const QTextBlock last = cursor.document()->findBlockByNumber(lastBlock);
    const int start = first.position();
    const int end = last.position() + last.text().length();

    QTextCursor replace(cursor.document());
    replace.setPosition(start);
    replace.setPosition(end, QTextCursor::KeepAnchor);
    replace.insertText(openFence + QLatin1Char('\n') + lines.join(QLatin1Char('\n'))
                       + QLatin1Char('\n') + closeFence);
    editor->setFocus();
}
