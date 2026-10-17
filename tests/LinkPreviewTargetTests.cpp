#include <gtest/gtest.h>
#include "utils/LinkPreviewTarget.h"

using namespace stc::links;

// ---------------------------------------------------------------------------------------------------------------------
// normalizedWebAddress
// ---------------------------------------------------------------------------------------------------------------------

TEST(NormalizedWebAddressTest, AcceptsHttpAndHttps)
{
    EXPECT_EQ(normalizedWebAddress("https://cpp0x.pl/kursy").value(), QUrl("https://cpp0x.pl/kursy"));
    EXPECT_EQ(normalizedWebAddress("http://example.com").value(), QUrl("http://example.com"));
    EXPECT_EQ(normalizedWebAddress("HTTPS://Example.com/A").value().host(), "example.com");
}

TEST(NormalizedWebAddressTest, TrimsAndAddsHttpsToWww)
{
    EXPECT_EQ(normalizedWebAddress("  https://cpp0x.pl  ").value(), QUrl("https://cpp0x.pl"));
    EXPECT_EQ(normalizedWebAddress("www.cpp0x.pl").value(), QUrl("https://www.cpp0x.pl"));
    EXPECT_EQ(normalizedWebAddress("WWW.cpp0x.pl/x").value().scheme(), "https");
}

TEST(NormalizedWebAddressTest, AcceptsUnencodedNationalLetters)
{
    const auto url = normalizedWebAddress("https://pl.wikipedia.org/wiki/Zażółć_gęślą_jaźń");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->host(), "pl.wikipedia.org");
}

TEST(NormalizedWebAddressTest, RejectsEverythingWhichIsNotAWebPage)
{
    EXPECT_FALSE(normalizedWebAddress("javascript:alert(1)"));
    EXPECT_FALSE(normalizedWebAddress("file:///etc/passwd"));
    EXPECT_FALSE(normalizedWebAddress("ftp://example.com/file"));
    EXPECT_FALSE(normalizedWebAddress("mailto:someone@example.com"));
    EXPECT_FALSE(normalizedWebAddress("data:text/html,<b>x</b>"));
    EXPECT_FALSE(normalizedWebAddress("about:blank"));
}

TEST(NormalizedWebAddressTest, RejectsBrokenAddresses)
{
    EXPECT_FALSE(normalizedWebAddress(""));
    EXPECT_FALSE(normalizedWebAddress("   "));
    EXPECT_FALSE(normalizedWebAddress("https://"));
    EXPECT_FALSE(normalizedWebAddress("not a link"));
    EXPECT_FALSE(normalizedWebAddress("cpp0x.pl")); // no scheme and no www: it can be anything
    EXPECT_FALSE(normalizedWebAddress("https://exa mple.com"));
}

// ---------------------------------------------------------------------------------------------------------------------
// previewTargetAt: [a href="..."]
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
const QString kTag = R"([a href="https://cpp0x.pl" name="Cpp0x"])";
const QString kLine = "See " + kTag + " here";
constexpr int kTagStart = 4;
} // namespace

TEST(PreviewTargetTagTest, FindsTheTagUnderThePosition)
{
    const auto target = previewTargetAt(kLine, kTagStart, false);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->url, QUrl("https://cpp0x.pl"));
    EXPECT_EQ(target->start, kTagStart);
    EXPECT_EQ(target->length, kTag.size());

    EXPECT_TRUE(previewTargetAt(kLine, kTagStart + 10, false).has_value());
    EXPECT_TRUE(previewTargetAt(kLine, kTagStart + kTag.size() - 1, false).has_value());
}

TEST(PreviewTargetTagTest, NothingBeforeOrAfterTheTag)
{
    EXPECT_FALSE(previewTargetAt(kLine, 0, false));
    EXPECT_FALSE(previewTargetAt(kLine, kTagStart - 1, false));
    EXPECT_FALSE(previewTargetAt(kLine, kTagStart + kTag.size(), false)); // the place right after the `]`
    EXPECT_FALSE(previewTargetAt(kLine, kLine.size(), false));
    EXPECT_FALSE(previewTargetAt(kLine, -1, false));
}

TEST(PreviewTargetTagTest, ChoosesTheRightTagOfSeveral)
{
    const QString line = R"([a href="https://one.pl"] and [a href="https://two.pl" name="Two"])";
    EXPECT_EQ(previewTargetAt(line, 3, false)->url, QUrl("https://one.pl"));
    EXPECT_EQ(previewTargetAt(line, line.indexOf("two") + 1, false)->url, QUrl("https://two.pl"));
    EXPECT_FALSE(previewTargetAt(line, line.indexOf(" and ") + 2, false));
}

TEST(PreviewTargetTagTest, WorksWithoutTheNameAttribute)
{
    const auto target = previewTargetAt(R"([a href="https://cpp0x.pl"])", 5, false);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->url, QUrl("https://cpp0x.pl"));
}

TEST(PreviewTargetTagTest, WwwAddressInTheTagGetsHttps)
{
    EXPECT_EQ(previewTargetAt(R"([a href="www.cpp0x.pl"])", 5, false)->url, QUrl("https://www.cpp0x.pl"));
}

TEST(PreviewTargetTagTest, OnlyWebPagesArePreviewed)
{
    EXPECT_FALSE(previewTargetAt(R"__([a href="javascript:alert(1)"])__", 5, false));
    EXPECT_FALSE(previewTargetAt(R"([a href="file:///etc/passwd"])", 5, false));
    EXPECT_FALSE(previewTargetAt(R"([a href="mailto:a@b.pl"])", 5, false));
}

TEST(PreviewTargetTagTest, OtherTagsAreNotLinks)
{
    EXPECT_FALSE(previewTargetAt(R"([img src="https://cpp0x.pl/a.png"])", 5, false));
    EXPECT_FALSE(previewTargetAt(R"([b]https://cpp0x.pl[/b])", 5, false));
}

// ---------------------------------------------------------------------------------------------------------------------
// previewTargetAt: addresses written in the text (plain text mode)
// ---------------------------------------------------------------------------------------------------------------------

TEST(PreviewTargetBareUrlTest, IgnoredUnlessAskedFor)
{
    EXPECT_FALSE(previewTargetAt("see https://cpp0x.pl please", 8, false));
}

TEST(PreviewTargetBareUrlTest, FindsTheAddressUnderThePosition)
{
    const QString line = "see https://cpp0x.pl/kursy please";
    const auto target = previewTargetAt(line, 8, true);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->url, QUrl("https://cpp0x.pl/kursy"));
    EXPECT_EQ(target->start, 4);
    EXPECT_EQ(target->length, QString("https://cpp0x.pl/kursy").size());

    EXPECT_FALSE(previewTargetAt(line, 2, true));
    EXPECT_FALSE(previewTargetAt(line, line.size() - 2, true));
}

TEST(PreviewTargetBareUrlTest, PunctuationAfterTheAddressIsNotPartOfIt)
{
    const QString line = "see https://cpp0x.pl.";
    EXPECT_TRUE(previewTargetAt(line, 10, true).has_value());
    EXPECT_EQ(previewTargetAt(line, 10, true)->url, QUrl("https://cpp0x.pl"));
    EXPECT_FALSE(previewTargetAt(line, line.size() - 1, true)); // the final dot
}

TEST(PreviewTargetBareUrlTest, WwwAddress)
{
    const auto target = previewTargetAt("go to www.cpp0x.pl now", 8, true);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->url, QUrl("https://www.cpp0x.pl"));
}

TEST(PreviewTargetBareUrlTest, EmailsAndOtherSchemesAreNotPreviewed)
{
    EXPECT_FALSE(previewTargetAt("write to someone@example.com today", 12, true));
    EXPECT_FALSE(previewTargetAt("get ftp://example.com/file now", 10, true));
}

TEST(PreviewTargetBareUrlTest, TheTagWinsOverTheAddressInsideIt)
{
    const auto target = previewTargetAt(kLine, kTagStart + 8, true);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target->start, kTagStart);
    EXPECT_EQ(target->length, kTag.size());
}
