#pragma once

#include <QList>
#include <QString>

/// One entry of the news feed of cpp0x.pl
struct NewsItem
{
    int threadId = 0; ///< number of the forum thread: https://cpp0x.pl/forum/temat/?id=<threadId>
    QString title;
};

/// What was found in the answer of the feed
struct NewsFeed
{
    QList<NewsItem> items;   ///< in the order of the feed
    bool wellFormed = false; ///< the answer is XML at all (an HTML page or an error text is not)
    /// The root element and the elements in it, to see the shape of the feed: "<cpp0x>: <news> <title>"
    QString structure;
};

/// Reads the feed. An entry is an element named item, entry, news, post, article, topic, thread (or their Polish
/// names). Its title is in the title element; its thread number in the first of: an element named id, thread,
/// thread_id, topic, topic_id, nr or guid with a number as text, else "id=<number>" in its link.
NewsFeed parseNewsFeed(const QByteArray& xml);

/// The page of the forum thread
QString forumThreadUrl(int threadId);

/// Which news the user has not opened yet. The first feed only sets the baseline (nothing is new then); a thread which
/// appears later is unread until it is opened. Opened threads are not unread again when they show up in the feed.
class NewsReadState
{
public:
    /// Compares a fresh feed with what is known
    void update(const QList<NewsItem>& items);
    void markRead(int threadId);
    bool isUnread(int threadId) const;

    bool hasBaseline() const { return baselineDone; }
    const QList<int>& knownThreads() const { return known; }
    const QList<int>& unreadThreads() const { return unread; }

    /// Restores the state saved earlier
    void restore(bool baseline, const QList<int>& knownIds, const QList<int>& unreadIds);

private:
    static constexpr int maxKnown = 500;

    bool baselineDone = false;
    QList<int> known;   ///< the newest first; a thread is known once it has been in a feed
    QList<int> unread;
};
