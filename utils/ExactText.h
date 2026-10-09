#pragma once

#include <QString>
#include <QTextDocument>

/// The text of a document exactly as the user has it, lines separated by '\n'.
///
/// QTextDocument::toPlainText() is made for showing text: it turns a non-breaking space (U+00A0) into a plain space
/// and the line separator U+2028 into a line break. Used for saving a file, for the backup or for comparing with what
/// is on the disk it silently changes the text ("twarde spacje" are gone) and shifts the line numbers.
/// toRawText() keeps both, only its paragraph separators have to become '\n'.
inline QString exactPlainText(const QTextDocument* document)
{
    QString text = document->toRawText();
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    return text;
}
