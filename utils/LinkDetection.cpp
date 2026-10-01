#include "LinkDetection.h"

#include <QRegularExpression>
#include <algorithm>

namespace stc::links
{
namespace
{
bool isTrailingPunctuation(QChar c)
{
    switch (c.unicode())
    {
    case u'.': case u',': case u';': case u':': case u'!': case u'?':
    case u'"': case u'\'': case u'>': case u']': case u'}': case u')':
    case u'\u201D': case u'\u2019': case u'\u00BB': // ” ’ »
        return true;
    default:
        return false;
    }
}

/// Cuts the characters from the end which most likely belong to the sentence around the address.
int trimmedLength(const QString& text, int start, int length)
{
    while (length > 0)
    {
        const QChar last = text[start + length - 1];
        if (!isTrailingPunctuation(last))
            break;

        if (last == u')')
        {
            // `https://en.wikipedia.org/wiki/Foo_(bar)` - keep the `)` which closes a `(` of the address
            const auto begin = text.constBegin() + start;
            const auto end = begin + length;
            if (std::count(begin, end, QChar(u'(')) >= std::count(begin, end, QChar(u')')))
                break;
        }
        --length;
    }
    return length;
}

bool overlaps(const QVector<LinkSpan>& spans, int start, int length)
{
    for (const auto& s : spans)
        if (start < s.start + s.length && s.start < start + length)
            return true;
    return false;
}
} // namespace

QVector<LinkSpan> findLinks(const QString& text, int from, int to)
{
    from = std::max(from, 0);
    to = (to < 0) ? text.size() : std::min<int>(to, text.size());
    if (from >= to)
        return {};

    // `(?<![\p{L}\p{N}])` - the address has to start at a word boundary (`xhttp://` is not an address)
    static const QRegularExpression urlRe(
        R"((?<![\p{L}\p{N}_])(?:(?:https?|ftp)://|www\.)[^\s<>"'\x{201C}\x{201D}\x{00AB}\x{00BB}\[\]{}|\\^`]+)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    static const QRegularExpression emailRe(
        R"((?<![\p{L}\p{N}_.%+\-@])(?:mailto:)?[A-Za-z0-9][A-Za-z0-9._%+\-]*@[A-Za-z0-9](?:[A-Za-z0-9\-]*[A-Za-z0-9])?(?:\.[A-Za-z0-9](?:[A-Za-z0-9\-]*[A-Za-z0-9])?)*\.[A-Za-z]{2,})",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);

    const QString fragment = text.left(to);
    QVector<LinkSpan> result;

    auto urls = urlRe.globalMatch(fragment, from);
    while (urls.hasNext())
    {
        const auto m = urls.next();
        const int matchStart = static_cast<int>(m.capturedStart());
        const int length = trimmedLength(fragment, matchStart, static_cast<int>(m.capturedLength()));

        // "www." or "https://" alone is not an address
        const int prefixLength = fragment.mid(matchStart, 8).startsWith(QLatin1String("www."), Qt::CaseInsensitive)
                                     ? 4
                                     : static_cast<int>(fragment.indexOf(QLatin1String("://"), matchStart)) - matchStart + 3;
        if (length > prefixLength)
            result.append({ matchStart, length, LinkKind::Url });
    }

    auto emails = emailRe.globalMatch(fragment, from);
    while (emails.hasNext())
    {
        const auto m = emails.next();
        const int matchStart = static_cast<int>(m.capturedStart());
        const int matchLength = static_cast<int>(m.capturedLength());
        if (!overlaps(result, matchStart, matchLength))
            result.append({ matchStart, matchLength, LinkKind::Email });
    }

    std::sort(result.begin(), result.end(), [](const LinkSpan& a, const LinkSpan& b) { return a.start < b.start; });
    return result;
}
} // namespace stc::links
