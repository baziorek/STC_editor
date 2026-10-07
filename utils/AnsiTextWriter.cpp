#include "AnsiTextWriter.h"

#include <QColor>
#include <QFont>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>

namespace
{
constexpr QChar kEscape{u'\x1b'};
constexpr qsizetype kMaxUnfinishedSequence = 4096; // a stray ESC must not swallow the rest of the output

/// The 16 basic colors; the dark variants are for a light background, the bright ones for a dark background
/// (the foreground of the text of the g++ messages has to be readable on both).
QColor basicColor(int index, bool darkBackground)
{
    static constexpr QRgb onLight[16] = {
        0x000000, 0xc62828, 0x2e7d32, 0x9a6700, 0x1565c0, 0x8e24aa, 0x00838f, 0x5f6368,
        0x424242, 0xd32f2f, 0x388e3c, 0xb26a00, 0x1976d2, 0xab47bc, 0x0097a7, 0x212121};
    static constexpr QRgb onDark[16] = {
        0x5c5c5c, 0xff6b68, 0x7fd962, 0xe5c07b, 0x6cb6ff, 0xd78bff, 0x56d4dd, 0xd0d0d0,
        0x8a8a8a, 0xff8a80, 0xa5e887, 0xffe08a, 0x90caf9, 0xe1a8ff, 0x80deea, 0xffffff};
    return QColor(darkBackground ? onDark[index & 15] : onLight[index & 15]);
}

/// Color from the 256 colors palette (`38;5;n`).
QColor color256(int n, bool darkBackground)
{
    if (n < 0 || n > 255)
        return {};
    if (n < 16)
        return basicColor(n, darkBackground);
    if (n < 232)
    {
        n -= 16;
        auto level = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
        return QColor(level(n / 36), level((n / 6) % 6), level(n % 6));
    }
    const int gray = 8 + (n - 232) * 10;
    return QColor(gray, gray, gray);
}
} // namespace

void AnsiTextWriter::reset()
{
    pending_.clear();
    format_ = QTextCharFormat();
}

void AnsiTextWriter::applySgr(const QString& parameters, bool darkBackground)
{
    const QStringList parts = parameters.isEmpty() ? QStringList{QStringLiteral("0")} : parameters.split(QLatin1Char(';'));
    QList<int> codes;
    for (const QString& part : parts)
        codes << part.toInt(); // an empty part is 0, as in the standard

    for (qsizetype i = 0; i < codes.size(); ++i)
    {
        const int code = codes[i];
        if (code == 0)
        {
            format_ = QTextCharFormat();
        }
        else if (code == 1)
        {
            format_.setFontWeight(QFont::Bold);
        }
        else if (code == 22)
        {
            format_.setFontWeight(QFont::Normal);
        }
        else if (code == 3 || code == 23)
        {
            format_.setFontItalic(code == 3);
        }
        else if (code == 4 || code == 24)
        {
            format_.setFontUnderline(code == 4);
        }
        else if (code >= 30 && code <= 37)
        {
            format_.setForeground(basicColor(code - 30, darkBackground));
        }
        else if (code >= 90 && code <= 97)
        {
            format_.setForeground(basicColor(code - 90 + 8, darkBackground));
        }
        else if (code == 39)
        {
            format_.clearForeground();
        }
        else if (code >= 40 && code <= 47)
        {
            format_.setBackground(basicColor(code - 40, darkBackground));
        }
        else if (code >= 100 && code <= 107)
        {
            format_.setBackground(basicColor(code - 100 + 8, darkBackground));
        }
        else if (code == 49)
        {
            format_.clearBackground();
        }
        else if (code == 38 || code == 48) // extended color: 38;5;n or 38;2;r;g;b
        {
            QColor color;
            if (i + 2 < codes.size() && codes[i + 1] == 5)
            {
                color = color256(codes[i + 2], darkBackground);
                i += 2;
            }
            else if (i + 4 < codes.size() && codes[i + 1] == 2)
            {
                color = QColor(codes[i + 2] & 255, codes[i + 3] & 255, codes[i + 4] & 255);
                i += 4;
            }
            if (color.isValid())
            {
                if (code == 38)
                    format_.setForeground(color);
                else
                    format_.setBackground(color);
            }
        }
        // everything else (blink, reverse, ...) is ignored
    }
}

void AnsiTextWriter::append(QPlainTextEdit* edit, const QString& chunk)
{
    pending_ += chunk;
    const bool darkBackground = edit->palette().base().color().lightness() < 128;

    QTextCursor cursor(edit->document()); // our own cursor: the selection and the position of the user stay untouched
    cursor.movePosition(QTextCursor::End);

    QString plain;
    auto flushPlain = [&] {
        if (!plain.isEmpty())
        {
            cursor.insertText(plain, format_);
            plain.clear();
        }
    };

    qsizetype i = 0;
    const qsizetype size = pending_.size();
    while (i < size)
    {
        const QChar ch = pending_[i];
        if (ch != kEscape)
        {
            plain += ch;
            ++i;
            continue;
        }

        if (i + 1 >= size) // ESC is the last character: the rest comes in the next chunk
            break;

        const QChar kind = pending_[i + 1];
        if (kind == QLatin1Char('['))
        {
            // CSI: ESC [ parameters (0x30-0x3F) intermediates (0x20-0x2F) final (0x40-0x7E)
            qsizetype j = i + 2;
            while (j < size && pending_[j].unicode() >= 0x20 && pending_[j].unicode() <= 0x3F)
                ++j;
            if (j >= size)
                break; // unfinished
            if (pending_[j].unicode() >= 0x40 && pending_[j].unicode() <= 0x7E)
            {
                if (pending_[j] == QLatin1Char('m'))
                {
                    flushPlain();
                    applySgr(pending_.mid(i + 2, j - (i + 2)), darkBackground);
                }
                // the other sequences (e.g. `ESC [ K` - erase the line, which g++ prints all the time) are dropped
                i = j + 1;
            }
            else
            {
                i = j; // not a valid sequence: drop the ESC [ and go on
            }
        }
        else if (kind == QLatin1Char(']'))
        {
            // OSC (e.g. hyperlinks): ESC ] ... BEL   or   ESC ] ... ESC backslash
            qsizetype j = i + 2;
            qsizetype end = -1;
            for (; j < size; ++j)
            {
                if (pending_[j] == QLatin1Char('\a'))
                {
                    end = j + 1;
                    break;
                }
                if (pending_[j] == kEscape && j + 1 < size && pending_[j + 1] == QLatin1Char('\\'))
                {
                    end = j + 2;
                    break;
                }
                if (pending_[j] == kEscape && j + 1 >= size)
                    break;
            }
            if (end < 0)
            {
                if (size - i > kMaxUnfinishedSequence)
                    i += 2; // garbage, give up
                else
                    break; // unfinished
            }
            else
            {
                i = end;
            }
        }
        else
        {
            i += 2; // any other two-character sequence
        }
    }

    flushPlain();
    pending_.remove(0, i);
    if (pending_.size() > kMaxUnfinishedSequence)
        pending_.clear();
}
