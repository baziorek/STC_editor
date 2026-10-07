#pragma once

#include <QString>
#include <QTextCharFormat>

class QPlainTextEdit;

/// Appends the output of a console program to a `QPlainTextEdit`, turning the ANSI escape sequences
/// (colors, bold, ... - what `g++ -fdiagnostics-color=always` prints) into the text formatting.
///
/// The escape sequences are not put into the document, so `QPlainTextEdit::toPlainText()` returns
/// the plain text, which is what is pasted into the (text) article.
///
/// The output arrives in chunks which can end in the middle of an escape sequence, so the writer
/// remembers the unfinished tail and the current formatting between the calls to `append()`.
class AnsiTextWriter
{
public:
    /// Forgets the formatting and the unfinished sequence; call it before the output of a new process.
    void reset();

    /// Appends the text at the end of the document of `edit` (the cursor of the user is not moved).
    void append(QPlainTextEdit* edit, const QString& chunk);

private:
    void applySgr(const QString& parameters, bool darkBackground);

    QString pending_;          ///< unfinished escape sequence from the end of the previous chunk
    QTextCharFormat format_;   ///< formatting set by the last SGR sequence
};
