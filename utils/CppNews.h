#pragma once

#include <QList>
#include <QString>

/// One entry of the news feed of cpp0x.pl
struct NewsItem
{
    int threadId = 0; ///< number of the forum thread: https://cpp0x.pl/forum/temat/?id=<threadId>
    QString title;
};

/// Reads the feed (RSS: <item>, Atom: <entry>) in its order. `ok` (if given) is false when the data is not an XML feed.
/// The thread number is taken from the first of: a child element named id, thread, thread_id, topic, topic_id,
/// nr or guid with a number as text; otherwise from "id=<number>" in the link.
QList<NewsItem> parseNewsFeed(const QByteArray& xml, bool* ok = nullptr);

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
