#pragma once

#include <QString>
#include <QVector>

namespace stc::links
{
enum class LinkKind
{
    Url,   ///< `http://...`, `https://...`, `ftp://...`, `www....`
    Email, ///< `name@host.tld`, also with the `mailto:` prefix
};

struct LinkSpan
{
    int start = 0;
    int length = 0;
    LinkKind kind = LinkKind::Url;

    bool operator==(const LinkSpan&) const = default;
};

/// Finds the web addresses and e-mail addresses in `[from, to)` of `text`, in the order of appearance.
/// The punctuation which ends a sentence (`.`, `,`, `)`, quotes...) is not a part of the address;
/// a `)` belongs to it only when the address has a matching `(` (like in Wikipedia's links).
/// An address which is inside of a web address (`https://user@host/`) is not reported once more as an e-mail.
QVector<LinkSpan> findLinks(const QString& text, int from = 0, int to = -1);
} // namespace stc::links
