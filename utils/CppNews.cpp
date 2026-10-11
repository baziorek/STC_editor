#include "CppNews.h"

#include <QRegularExpression>
#include <QStringList>
#include <QXmlStreamReader>
#include <optional>

namespace
{
bool isItemElement(const QString& name)
{
    static const QStringList names = { "item", "entry", "news", "newsitem", "news_item", "post", "article", "topic",
                                       "thread", "wpis", "temat", "artykul", "aktualnosc" };
    return names.contains(name, Qt::CaseInsensitive);
}

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

/// Reads the entry whose start element was just read; stops after its end element
NewsItem readItem(QXmlStreamReader& reader, const QString& elementName)
{
    NewsItem item;
    std::optional<int> threadId;
    QString link;

    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isEndElement() && reader.name().toString() == elementName)
            break;
        if (!reader.isStartElement())
            continue;

        // copied: a QStringView from the reader is not valid after the next readNext()
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

    if (!threadId)
        threadId = threadIdFromLink(link);
    item.threadId = threadId.value_or(0);
    return item;
}
} // namespace

NewsFeed parseNewsFeed(const QByteArray& xml)
{
    NewsFeed feed;
    QXmlStreamReader reader(xml);

    QString rootName;
    QStringList childNames; // the elements directly in the root, once each
    int depth = 0;

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isEndElement())
        {
            --depth;
            continue;
        }
        if (!reader.isStartElement())
            continue;

        ++depth;
        const QString name = reader.name().toString();
        if (depth == 1 && rootName.isEmpty())
            rootName = name;
        else if (depth == 2 && !childNames.contains(name) && childNames.size() < 10)
            childNames.append(name);

        if (isItemElement(name))
        {
            const NewsItem item = readItem(reader, name);
            if (item.threadId > 0)
                feed.items.append(item);
            --depth; // readItem() has read the end element of the entry
        }
    }

    feed.wellFormed = !reader.hasError() && !rootName.isEmpty();
    if (!rootName.isEmpty())
    {
        QStringList shownChildren;
        for (const QString& child : std::as_const(childNames))
            shownChildren << QStringLiteral("<%1>").arg(child);
        feed.structure = QStringLiteral("<%1>: %2").arg(rootName, shownChildren.join(QLatin1Char(' ')));
    }
    return feed;
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
