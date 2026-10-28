#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStringList>
#include <QTextCursor>
#include "StcInlineTagToggle.h"

namespace
{
// `[tag attributes]selection[/tag]`; without a selection the cursor lands between the tags
void surroundWithTag(QPlainTextEdit* editor, const QString& name, const QString& attributes)
{
    QTextCursor cursor = editor->textCursor();
    QString selected = cursor.selectedText();
    selected.replace(QChar::ParagraphSeparator, u'\n');

    const QString close = "[/" + name + "]";
    cursor.insertText("[" + name + attributes + "]" + selected + close);
    if (selected.isEmpty())
    {
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, close.size());
    }
    editor->setTextCursor(cursor);
}

// Inserts `text`; the cursor lands `back` characters before the end of it (when back > 0)
void insertAndMoveBack(QPlainTextEdit* editor, const QString& text, int back)
{
    QTextCursor cursor = editor->textCursor();
    cursor.insertText(text);
    if (back > 0)
    {
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, back);
    }
    editor->setTextCursor(cursor);
}

// `[a href="link" name="description"]`: the address is taken from the selected text, the rest is the name
void insertLink(QPlainTextEdit* editor)
{
    static const QRegularExpression urlRe(R"((https?://|www\.)\S+)");
    constexpr QLatin1String begin{R"([a href=")"};
    constexpr QLatin1String end{R"("])"};

    QString selected = editor->textCursor().selectedText().trimmed();
    selected.replace(QChar::ParagraphSeparator, u' ');
    if (selected.isEmpty())
    {
        insertAndMoveBack(editor, begin + end, end.size()); // cursor inside of href
        return;
    }

    selected.replace(u'"', u'\''); // a quote would end the attribute
    const QRegularExpressionMatch match = urlRe.match(selected);
    if (!match.hasMatch())
    {
        const QString text = begin + QString() + R"(" name=")" + selected + end;
        insertAndMoveBack(editor, text, QString(R"(" name=")" + selected + end).size()); // cursor inside of href
        return;
    }

    const QString link = match.captured(0);
    const QString left = selected.left(match.capturedStart()).trimmed();
    const QString right = selected.mid(match.capturedEnd()).trimmed();
    const QString name = (left + " " + right).trimmed();

    QString text = begin + link;
    if (!name.isEmpty())
    {
        text += R"(" name=")" + name;
    }
    insertAndMoveBack(editor, text + end, 0);
}

// `[img src="file" opis="description"]`: the file is a word with a known extension in the selected text
void insertImage(QPlainTextEdit* editor)
{
    static const QStringList imageExtensions = {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg", ".avif", ".tiff"};
    constexpr QLatin1String begin{R"([img src=")"};
    constexpr QLatin1String end{R"("])"};
    constexpr QLatin1String description{R"(" opis=")"};

    QString selected = editor->textCursor().selectedText().trimmed();
    selected.replace(QChar::ParagraphSeparator, u' ');
    if (selected.isEmpty())
    {
        insertAndMoveBack(editor, begin + end, end.size()); // cursor inside of src
        return;
    }

    QStringList words = selected.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    int imageIndex = -1;
    for (int i = 0; i < words.size() && imageIndex < 0; ++i)
    {
        for (const QString& extension : imageExtensions)
        {
            if (words[i].endsWith(extension, Qt::CaseInsensitive))
            {
                imageIndex = i;
                break;
            }
        }
    }

    if (imageIndex < 0)
    {
        insertAndMoveBack(editor, begin + description + selected + end, QString(description + selected + end).size());
        return;
    }
    const QString file = words.takeAt(imageIndex);
    insertAndMoveBack(editor, words.isEmpty() ? begin + file + end : begin + file + description + words.join(' ') + end, 0);
}

bool textAround(QPlainTextEdit* editor, int from, int length, const QString& expected)
{
    if (from < 0)
    {
        return false;
    }
    QTextCursor probe(editor->document());
    probe.setPosition(from);
    probe.setPosition(from + length, QTextCursor::KeepAnchor);
    return probe.selectedText() == expected && probe.selectionEnd() - probe.selectionStart() == length;
}

void selectRange(QPlainTextEdit* editor, int from, int length)
{
    QTextCursor cursor(editor->document());
    cursor.setPosition(from);
    cursor.setPosition(from + length, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}
}

void stcToggleInlineTag(QPlainTextEdit* editor, const QString& tag)
{
    if (!editor)
    {
        return;
    }
    const QString open = "[" + tag + "]";
    const QString close = "[/" + tag + "]";

    QTextCursor cursor = editor->textCursor();
    QString selected = cursor.selectedText();
    selected.replace(QChar::ParagraphSeparator, u'\n');

    int lead = 0;
    while (lead < selected.size() && selected[lead].isSpace())
        ++lead;
    int trail = 0;
    while (trail < selected.size() - lead && selected[selected.size() - 1 - trail].isSpace())
        ++trail;
    const QString core = selected.mid(lead, selected.size() - lead - trail);

    if (core.isEmpty())
    {
        // nothing (but maybe spaces) selected: an empty pair, the cursor lands between the tags
        cursor.insertText(open + close);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, close.size());
        editor->setTextCursor(cursor);
        return;
    }

    const int start = cursor.selectionStart();

    // 1. the selection contains the tags -> remove them
    if (core.startsWith(open) && core.endsWith(close) && core.size() >= open.size() + close.size())
    {
        const QString inner = core.mid(open.size(), core.size() - open.size() - close.size());
        cursor.insertText(selected.left(lead) + inner + selected.right(trail));
        selectRange(editor, start + lead, inner.size());
        return;
    }

    // 2. the selection is inside of the tags -> remove the tags around
    const int end = cursor.selectionEnd();
    if (textAround(editor, start - open.size(), open.size(), open) && textAround(editor, end, close.size(), close))
    {
        cursor.beginEditBlock();
        cursor.setPosition(end);
        cursor.setPosition(end + close.size(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setPosition(start - open.size());
        cursor.setPosition(start, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.endEditBlock();
        selectRange(editor, start - open.size(), end - start);
        return;
    }

    // 3. default: surround (spaces at the edges of the selection stay outside of the tags)
    cursor.insertText(selected.left(lead) + open + core + close + selected.right(trail));
    selectRange(editor, start + lead + open.size(), core.size());
}

void stcApplyTag(QPlainTextEdit* editor, StcTags tag)
{
    if (!editor)
    {
        return;
    }

    switch (tag)
    {
    case StcTags::BOLD:
    case StcTags::ITALIC:
    case StcTags::UNDERLINED:
    case StcTags::STRUCK_OUT:
    case StcTags::SUBSCRIPT:
    case StcTags::SUPSCRIPT:
    case StcTags::TELE_TYPE:
    case StcTags::CODE:
    case StcTags::CPP:
    case StcTags::PY:
    case StcTags::QUOTE:
    case StcTags::H1:
    case StcTags::H2:
    case StcTags::H3:
    case StcTags::H4:
        stcToggleInlineTag(editor, tagsClasses[tag]);
        break;
    case StcTags::DIV:
        surroundWithTag(editor, tagsClasses[StcTags::DIV], {});
        break;
    case StcTags::DIV_TIP:
        surroundWithTag(editor, tagsClasses[StcTags::DIV], R"( class="tip")");
        break;
    case StcTags::DIV_WARNING:
        surroundWithTag(editor, tagsClasses[StcTags::DIV], R"( class="uwaga")");
        break;
    case StcTags::PKT:
        surroundWithTag(editor, tagsClasses[StcTags::PKT], " ext");
        break;
    case StcTags::A_HREF:
        insertLink(editor);
        break;
    case StcTags::IMG:
        insertImage(editor);
        break;
    default: // CSV and RUN have no sense inside of a cell
        break;
    }
}
