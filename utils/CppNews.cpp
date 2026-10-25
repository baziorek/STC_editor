#include "CppNews.h"

#include <QRegularExpression>
#include <QStringList>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <optional>
#include <QChar>
#include <QLatin1Char>
#include <QLatin1String>
#include <QRegularExpressionMatch>
#include <QStringLiteral>
#include <QStringView>
#include <QXmlStreamAttribute>
#include <Qt>
#include <utility>

class QByteArray;

namespace
{
constexpr int maxShownNames = 10;

bool isItemElement(const QString& name)
{
    static const QStringList names = { "item", "entry", "news", "newsitem", "news_item", "post", "article", "topic",
                                       "thread", "wpis", "temat", "artykul", "aktualnosc" };
    return names.contains(name, Qt::CaseInsensitive);
}

bool isThreadIdName(const QString& name)
{
    static const QStringList names = { "id", "thread", "thread_id", "topic", "topic_id", "nr", "guid" };
    return names.contains(name, Qt::CaseInsensitive);
}

std::optional<int> numberIn(const QString& text)
{
    bool numeric = false;
    const int value = text.trimmed().toInt(&numeric);
    if (!numeric)
        return std::nullopt;
    return value;
}

std::optional<int> threadIdFromLink(const QString& link)
{
    static const QRegularExpression idParameter(QStringLiteral("[?&]id=(\\d+)"));
    const auto match = idParameter.match(link);
    if (!match.hasMatch())
        return std::nullopt;
    return match.captured(1).toInt();
}

/// The thread number as an attribute of the entry element itself, e.g. <Topic Id="29289" ...>
std::optional<int> threadIdFromAttributes(const QXmlStreamAttributes& attributes)
{
    for (const QXmlStreamAttribute& attribute : attributes)
    {
        if (!isThreadIdName(attribute.qualifiedName().toString()))
            continue;
        if (const auto id = numberIn(attribute.value().toString()))
            return id;
    }
    return std::nullopt;
}

/// What was read from one entry, besides the entry itself: the names are for the diagnostic only
struct ItemReading
{
    NewsItem item;
    QStringList attributeNames;
    QStringList childNames;
};

/// Reads the entry whose start element was just read; stops after its end element
ItemReading readItem(QXmlStreamReader& reader, const QString& elementName)
{
    ItemReading reading;

    // the attributes are only available while the start element is current
    const QXmlStreamAttributes attributes = reader.attributes();
    for (const QXmlStreamAttribute& attribute : attributes)
    {
        if (reading.attributeNames.size() < maxShownNames)
            reading.attributeNames << attribute.qualifiedName().toString();
    }

    std::optional<int> threadId = threadIdFromAttributes(attributes);
    std::optional<int> elementThreadId;
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
        if (!reading.childNames.contains(child) && reading.childNames.size() < maxShownNames)
            reading.childNames << child;

        if (child.compare(QStringLiteral("title"), Qt::CaseInsensitive) == 0)
        {
            reading.item.title = reader.readElementText().trimmed();
        }
        else if (child.compare(QStringLiteral("link"), Qt::CaseInsensitive) == 0)
        {
            // Atom keeps the address in an attribute, RSS in the text
            const QString href = reader.attributes().value(QLatin1String("href")).toString();
            const QString text = reader.readElementText().trimmed();
            link = href.isEmpty() ? text : href;
        }
        else if (isThreadIdName(child) && !elementThreadId)
        {
            elementThreadId = numberIn(reader.readElementText());
        }
    }

    if (!threadId)
        threadId = elementThreadId;
    if (!threadId)
        threadId = threadIdFromLink(link);
    reading.item.threadId = threadId.value_or(0);
    return reading;
}

QString describeItem(const QString& elementName, const ItemReading& reading)
{
    return QStringLiteral("item <%1>; attributes: %2; elements: %3")
        .arg(elementName, reading.attributeNames.join(QLatin1Char(' ')), reading.childNames.join(QLatin1Char(' ')));
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
        else if (depth == 2 && !childNames.contains(name) && childNames.size() < maxShownNames)
            childNames.append(name);

        if (isItemElement(name))
        {
            const ItemReading reading = readItem(reader, name);
            if (feed.itemShape.isEmpty())
                feed.itemShape = describeItem(name, reading);
            if (reading.item.threadId > 0)
                feed.items.append(reading.item);
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
