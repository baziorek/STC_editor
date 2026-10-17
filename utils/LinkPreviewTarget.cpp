#include "LinkPreviewTarget.h"

#include <QRegularExpression>
#include "LinkDetection.h"
#include "stcSyntaxPatterns.h"

namespace stc::links
{
std::optional<QUrl> normalizedWebAddress(const QString& text)
{
    QString address = text.trimmed();
    if (address.startsWith(u"www.", Qt::CaseInsensitive))
        address.prepend(u"https://");

    // Tolerant: the addresses of Polish articles contain unencoded letters, e.g. `.../wiki/Zażółć`
    const QUrl url(address, QUrl::TolerantMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || (scheme != u"http" && scheme != u"https") || url.host().isEmpty())
        return std::nullopt;
    return url;
}

namespace
{
bool isInside(int position, int start, int length)
{
    return position >= start && position < start + length;
}

std::optional<PreviewTarget> targetFromTag(const QString& line, int position)
{
    auto tags = stc::syntax::anchorRe.globalMatch(line);
    while (tags.hasNext())
    {
        const auto tag = tags.next();
        const int start = static_cast<int>(tag.capturedStart(0));
        const int length = static_cast<int>(tag.capturedLength(0));
        if (!isInside(position, start, length))
            continue;

        if (const auto url = normalizedWebAddress(tag.captured(1)))
            return PreviewTarget{*url, start, length};
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<PreviewTarget> targetFromBareUrl(const QString& line, int position)
{
    for (const auto& link : findLinks(line))
    {
        if (link.kind != LinkKind::Url || !isInside(position, link.start, link.length))
            continue;

        if (const auto url = normalizedWebAddress(line.mid(link.start, link.length)))
            return PreviewTarget{*url, link.start, link.length};
        return std::nullopt;
    }
    return std::nullopt;
}
} // namespace

std::optional<PreviewTarget> previewTargetAt(const QString& line, int position, bool includeBareUrls)
{
    if (position < 0)
        return std::nullopt;

    if (const auto tagTarget = targetFromTag(line, position))
        return tagTarget;
    return includeBareUrls ? targetFromBareUrl(line, position) : std::nullopt;
}
} // namespace stc::links
