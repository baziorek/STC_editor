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
// The feed of cpp0x.pl as the site gives it (cut to four threads): the number of the thread is the Id attribute of
// Topic, and its title is in a Title element with a capital letter
const QByteArray forumFeed = R"(<?xml version="1.0" encoding="UTF-8" ?>
<CodeDesk>
    <Information>Materials from this application come from cpp0x.pl.</Information>
    <Forum>
        <TopicContainer>
            <Topic Id="29289" ParentId="2" AccessId="0" CreateUserId="22490" CreateUserNick="czerwonymoskit" LastUserId="22490" LastUserNick="czerwonymoskit" CreateDT="2026-09-24 17:34:20" UpdateDT="2026-09-27 11:34:03" PostCount="18" OrderKey="0" IsOpen="1" Visited="0" FakeVisited="0" LastPageNo="1">
                <Title>C++, Dziwne zachowanie zmiennych globalnych</Title>
            </Topic>
            <Topic Id="29272" ParentId="12" AccessId="0" CreateUserId="24438" CreateUserNick="tBane" LastUserId="24438" LastUserNick="tBane" CreateDT="2026-08-21 20:45:55" UpdateDT="2026-09-09 13:07:26" PostCount="7" OrderKey="0" IsOpen="1" Visited="0" FakeVisited="0" LastPageNo="1">
                <Title>Filmy o AI</Title>
            </Topic>
            <Topic Id="29262" ParentId="3" AccessId="0" CreateUserId="24438" CreateUserNick="tBane" LastUserId="24438" LastUserNick="tBane" CreateDT="2026-06-02 14:31:18" UpdateDT="2026-06-02 15:32:33" PostCount="2" OrderKey="0" IsOpen="0" Visited="0" FakeVisited="0" LastPageNo="1">
                <Title>[SFML 3.0] Ramka z podpisem</Title>
            </Topic>
            <Topic Id="29253" ParentId="2" AccessId="0" CreateUserId="24438" CreateUserNick="tBane" LastUserId="24438" LastUserNick="tBane" CreateDT="2026-04-04 18:59:42" UpdateDT="2026-04-06 23:21:58" PostCount="10" OrderKey="0" IsOpen="0" Visited="0" FakeVisited="0" LastPageNo="1">
                <Title>Destruktor i Try .. Catch</Title>
            </Topic>
        </TopicContainer>
    </Forum>
</CodeDesk>
)";
} // namespace

TEST(CppNewsParseTest, ForumFeedGivesTheThreadNumberFromTheIdAttribute)
{
    const NewsFeed feed = parseNewsFeed(forumFeed);

    ASSERT_TRUE(feed.wellFormed);
    ASSERT_EQ(feed.items.size(), 4);
    EXPECT_EQ(feed.items[0].threadId, 29289);
    EXPECT_EQ(feed.items[0].title, "C++, Dziwne zachowanie zmiennych globalnych");
    EXPECT_EQ(feed.items[1].threadId, 29272);
    EXPECT_EQ(feed.items[2].threadId, 29262);
    EXPECT_EQ(feed.items[3].threadId, 29253);
    EXPECT_EQ(feed.items[3].title, "Destruktor i Try .. Catch");
    EXPECT_EQ(forumThreadUrl(feed.items[0].threadId), "https://cpp0x.pl/forum/temat/?id=29289");
}

TEST(CppNewsParseTest, AnEntryWithoutANumberIsShownInTheDiagnostic)
{
    const NewsFeed feed = parseNewsFeed(
        QByteArray("<CodeDesk><Forum><Topic Name=\"x\"><Title>t</Title></Topic></Forum></CodeDesk>"));

    EXPECT_TRUE(feed.wellFormed);
    EXPECT_TRUE(feed.items.isEmpty());
    EXPECT_EQ(feed.structure, "<CodeDesk>: <Forum>");
    EXPECT_EQ(feed.itemShape, "item <Topic>; attributes: Name; elements: Title");
}

TEST(CppNewsParseTest, RssItemsKeepTheirOrderAndNumberOfThread)
{
    const NewsFeed feed = parseNewsFeed(rssFeed);

    ASSERT_TRUE(feed.wellFormed);
    ASSERT_EQ(feed.items.size(), 2);
    EXPECT_EQ(feed.items[0].title, "Newest thread");
    EXPECT_EQ(feed.items[0].threadId, 29300); // from the <id> element
    EXPECT_EQ(feed.items[1].threadId, 29289); // no element: from the link
}

TEST(CppNewsParseTest, AtomEntriesGiveTheNumberFromTheHrefOfTheLink)
{
    const NewsFeed feed = parseNewsFeed(atomFeed);

    ASSERT_TRUE(feed.wellFormed);
    ASSERT_EQ(feed.items.size(), 1);
    EXPECT_EQ(feed.items[0].threadId, 123);
}

TEST(CppNewsParseTest, AnyRootWithNewsElementsIsRead)
{
    // the root is not rss/feed: the news are still found, and the structure says what the feed holds
    const NewsFeed feed = parseNewsFeed(
        QByteArray("<cpp0x><news><title>Thread</title><nr>77</nr></news><news><title>Other</title></news></cpp0x>"));

    EXPECT_TRUE(feed.wellFormed);
    ASSERT_EQ(feed.items.size(), 1); // the one without a number is left out
    EXPECT_EQ(feed.items[0].threadId, 77);
    EXPECT_EQ(feed.structure, "<cpp0x>: <news>");
}

TEST(CppNewsParseTest, PlainTextIsNotAFeed)
{
    const NewsFeed feed = parseNewsFeed("Access denied");
    EXPECT_FALSE(feed.wellFormed);
    EXPECT_TRUE(feed.items.isEmpty());
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
