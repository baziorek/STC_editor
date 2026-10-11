// The news feed of cpp0x.pl: reading the threads, and which of them are new. No network, no widget.

#include <gtest/gtest.h>

#include <QByteArray>

#include "utils/CppNews.h"

namespace
{
const QByteArray rssFeed = R"(<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"><channel>
  <title>cpp0x.pl</title>
  <item>
    <title>Newest thread</title>
    <link>https://cpp0x.pl/forum/temat/?id=29300</link>
    <id>29300</id>
  </item>
  <item>
    <title>Older thread</title>
    <link>https://cpp0x.pl/forum/temat/?id=29289</link>
  </item>
</channel></rss>)";

const QByteArray atomFeed = R"(<?xml version="1.0" encoding="utf-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <entry><title>Atom thread</title><link href="https://cpp0x.pl/forum/temat/?id=123"/></entry>
</feed>)";
} // namespace

TEST(CppNewsParseTest, RssItemsKeepTheirOrderAndNumberOfThread)
{
    bool ok = false;
    const auto items = parseNewsFeed(rssFeed, &ok);

    ASSERT_TRUE(ok);
    ASSERT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].title, "Newest thread");
    EXPECT_EQ(items[0].threadId, 29300); // from the <id> element
    EXPECT_EQ(items[1].threadId, 29289); // no element: from the link
}

TEST(CppNewsParseTest, AtomEntriesGiveTheNumberFromTheHrefOfTheLink)
{
    bool ok = false;
    const auto items = parseNewsFeed(atomFeed, &ok);

    ASSERT_TRUE(ok);
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].threadId, 123);
}

TEST(CppNewsParseTest, NotAFeedIsReportedAndGivesNothing)
{
    bool ok = true;
    EXPECT_TRUE(parseNewsFeed("<html><body>Login</body></html>", &ok).isEmpty());
    EXPECT_FALSE(ok);
}

TEST(CppNewsParseTest, ForumAddressOfAThread)
{
    EXPECT_EQ(forumThreadUrl(29289), "https://cpp0x.pl/forum/temat/?id=29289");
}

namespace
{
QList<NewsItem> feed(std::initializer_list<int> ids)
{
    QList<NewsItem> items;
    for (const int id : ids)
        items.append(NewsItem{ id, QStringLiteral("t") });
    return items;
}
}

TEST(CppNewsReadStateTest, TheFirstFeedOnlySetsTheBaseline)
{
    NewsReadState state;
    state.update(feed({ 3, 2, 1 }));

    EXPECT_TRUE(state.hasBaseline());
    EXPECT_TRUE(state.unreadThreads().isEmpty());
}

TEST(CppNewsReadStateTest, ANewThreadIsUnreadUntilItIsOpened)
{
    NewsReadState state;
    state.update(feed({ 2, 1 }));

    state.update(feed({ 3, 2, 1 }));
    EXPECT_TRUE(state.isUnread(3));
    EXPECT_FALSE(state.isUnread(2));

    state.update(feed({ 3, 2, 1 })); // still there, still unread
    EXPECT_TRUE(state.isUnread(3));

    state.markRead(3);
    state.update(feed({ 3, 2, 1 })); // read threads do not come back as new
    EXPECT_FALSE(state.isUnread(3));
}

TEST(CppNewsReadStateTest, SeveralNewThreadsKeepTheirOrderNewestFirst)
{
    NewsReadState state;
    state.update(feed({ 1 }));
    state.update(feed({ 4, 3, 2, 1 }));

    EXPECT_EQ(state.unreadThreads(), (QList<int>{ 4, 3, 2 }));
}

TEST(CppNewsReadStateTest, RestoredStateKeepsWhatWasUnread)
{
    NewsReadState state;
    state.restore(true, { 5, 4 }, { 5 });
    state.update(feed({ 5, 4 }));

    EXPECT_TRUE(state.isUnread(5));
    EXPECT_FALSE(state.isUnread(4));
}
