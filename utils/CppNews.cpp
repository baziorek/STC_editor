#include "CppNews.h"

#include <QRegularExpression>
#include <QStringList>
#include <QXmlStreamReader>
#include <optional>

namespace
{
bool isThreadIdElement(const QString& name)
{
    static const QStringList names = { "id", "thread", "thread_id", "topic", "topic_id", "nr", "guid" };
    return names.contains(name, Qt::CaseInsensitive);
}

std::optional<int> threadIdFromLink(const QString& link)
{
    static const QRegularExpression idParameter(QStringLiteral("[?&]id=(\\d+)"));
    const auto match = idParameter.match(link);
    if (!match.hasMatch())
        return std::nullopt;
    return match.captured(1).toInt();
}

std::optional<int> numberInText(const QString& text)
{
    bool numeric = false;
    const int value = text.trimmed().toInt(&numeric);
    if (!numeric)
        return std::nullopt;
    return value;
}
} // namespace

QList<NewsItem> parseNewsFeed(const QByteArray& xml, bool* ok)
{
    QList<NewsItem> items;
    QXmlStreamReader reader(xml);
    bool sawFeed = false;

    while (!reader.atEnd())
    {
        reader.readNext();
        if (!reader.isStartElement())
            continue;

        // copied: a QStringView from the reader is not valid after the next readNext()
        const QString name = reader.name().toString();
        if (name == QLatin1String("rss") || name == QLatin1String("feed") || name == QLatin1String("channel"))
            sawFeed = true;

        if (name != QLatin1String("item") && name != QLatin1String("entry"))
            continue;

        NewsItem item;
        std::optional<int> threadId;
        QString link;

        // the children of the entry, up to its end
        while (!reader.atEnd())
        {
            reader.readNext();
            if (reader.isEndElement() && reader.name().toString() == name)
                break;
            if (!reader.isStartElement())
                continue;

            const QString child = reader.name().toString();
            if (child == QLatin1String("title"))
            {
                item.title = reader.readElementText().trimmed();
            }
            else if (child == QLatin1String("link"))
            {
                // Atom keeps the address in an attribute, RSS in the text
                const QString href = reader.attributes().value(QLatin1String("href")).toString();
                const QString text = reader.readElementText().trimmed();
                link = href.isEmpty() ? text : href;
            }
            else if (isThreadIdElement(child) && !threadId)
            {
                threadId = numberInText(reader.readElementText());
            }
        }

        // no number as an element: the id in the address of the thread
        if (!threadId)
            threadId = threadIdFromLink(link);

        item.threadId = threadId.value_or(0);
        if (item.threadId > 0)
            items.append(item);
    }

    if (ok)
        *ok = !reader.hasError() && sawFeed;
    return items;
}

QString forumThreadUrl(int threadId)
{
    return QStringLiteral("https://cpp0x.pl/forum/temat/?id=%1").arg(threadId);
}

void NewsReadState::update(const QList<NewsItem>& items)
{
    if (!baselineDone)
    {
        // the first feed: what is in it now is what was there before, nothing is new
        for (const NewsItem& item : items)
        {
            if (!known.contains(item.threadId))
                known.append(item.threadId);
        }
        baselineDone = true;
        return;
    }

    // newest first, as the feed gives them: a thread the user has not seen yet is added to the unread ones
    for (int i = items.size() - 1; i >= 0; --i)
    {
        const int threadId = items[i].threadId;
        if (known.contains(threadId))
            continue;

        known.prepend(threadId);
        unread.prepend(threadId);
    }

    while (known.size() > maxKnown)
        known.removeLast();
}

void NewsReadState::markRead(int threadId)
{
    unread.removeAll(threadId);
}

bool NewsReadState::isUnread(int threadId) const
{
    return unread.contains(threadId);
}

void NewsReadState::restore(bool baseline, const QList<int>& knownIds, const QList<int>& unreadIds)
{
    baselineDone = baseline;
    known = knownIds;
    unread = unreadIds;
}
