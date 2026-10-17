// Tests generated with ChatGPT
#include <algorithm>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "checkers/PairedTagsChecker.h"

// Test that correctly closed tags do not return any errors
TEST(PairedTagsCheckerTest, NoErrorsForCorrectlyClosedTags)
{
    std::string text = "Sample text with [b]bold[/b] and [i]italic[/i].\n"
                       "And a correct [code]code block[/code].";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty());
}

// Test that unclosed tags return appropriate errors
TEST(PairedTagsCheckerTest, ErrorsForUnclosedTags)
{
    std::string text = "Unclosed tag: [b]without closure.\n"
                       "And another [i]tag.\n";
    auto errors = PairedTagsChecker::checkTags(text);
    ASSERT_EQ(errors.size(), 2);
    EXPECT_EQ(errors[0].line, 1) << errors[0].line << ": " << errors[0].errorText;
    EXPECT_EQ(errors[1].line, 2) << errors[1].line << ": " << errors[1].errorText;
}

// Test that single 'img' tags are correctly ignored
TEST(PairedTagsCheckerTest, IgnoreImgTag)
{
    std::string text = "This is an image: [img] that doesn't need closure.\n"
                       "But this text [b]needs closure[/b].";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty());
}

// Test that mismatched tags return appropriate errors
TEST(PairedTagsCheckerTest, ErrorsForMismatchedTags)
{
    std::string text = "Start [b]bold[/i] and [i]italic[/b].";
    auto errors = PairedTagsChecker::checkTags(text);
    ASSERT_GT(errors.size(), 2);
    EXPECT_EQ(errors[0].line, 1) << errors[0].line << ": " << errors[0].errorText;
    EXPECT_EQ(errors[1].line, 1) << errors[1].line << ": " << errors[1].errorText;
}

// Test that correctly closed tags inside [run] do not return errors ([run] needs [pkt ext] or [csv ext] around it)
TEST(PairedTagsCheckerTest, IgnoreTagsInsideRun)
{
    std::string text = "Normal text [b]bold[/b].\n"
                       "[pkt ext][run] This text can contain [i] other tags [/i] and still work [/run][/pkt]\n"
                       "[i]Italic[/i] after run.";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty()) << "size: " << errors.size();
}

// Test that unclosed tags inside [run] are correctly reported
TEST(PairedTagsCheckerTest, ReportUnclosedTagsInsideRun)
{
    std::string text = "[pkt ext][run] Unclosed [b]tag inside run[/run][/pkt].";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_FALSE(errors.empty()) << "size: " << errors.size();
}

// Test that tags inside [code], [cpp], [py] are ignored
TEST(PairedTagsCheckerTest, IgnoreTagsInsideCode)
{
    std::string text = "Code [code]contains [b]unmatched tags[/i][/code] and is ignored.";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty());
}

// Test that unmatched closing tags are reported
TEST(PairedTagsCheckerTest, ReportUnmatchedClosingTags)
{
    std::string text = "Unmatched closing tag [/b] here.";
    auto errors = PairedTagsChecker::checkTags(text);
    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].line, 1);
}

// Test that tags with attributes are correctly parsed
TEST(PairedTagsCheckerTest, CorrectParsingOfTagsWithAttributes)
{
    std::string text = R"(Text with a tag [div class="example"]div[/div] and [a href="www.cpp0x.pl" name="link"])";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty()) << "size: " << errors.size();
}

namespace
{
bool anyErrorContains(const std::vector<PairedTagsChecker::TagError>& errors, const std::string& fragment)
{
    return std::ranges::any_of(errors, [&](const auto& error) { return error.errorText.find(fragment) != std::string::npos; });
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// [run] placement: it works only in [pkt ext] / [csv ext]
// ---------------------------------------------------------------------------------------------------------------------

TEST(PairedTagsCheckerRunTest, RunInsideExtendedPktCsvIsCorrect)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[pkt ext]one\n[run]two\nlines[/run]\n[/pkt]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[pkt extended][run]x[/run][/pkt]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[csv ext header]Head;\n[run][b]ok[/b][/run];[/csv]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[csv extended][run]ok[/run];[/csv]").empty());
}

TEST(PairedTagsCheckerRunTest, RunOutsideOfPktAndCsvIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("Some text\n[run]alone[/run]\nmore text");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].line, 2);
    EXPECT_EQ(errors[0].positionInLine, 0);
    EXPECT_TRUE(anyErrorContains(errors, "outside"));
}

TEST(PairedTagsCheckerRunTest, RunInsideNotExtendedPktIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[pkt]\n[run]x[/run]\n[/pkt]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].line, 2);
    EXPECT_TRUE(anyErrorContains(errors, "[pkt ext]"));
}

TEST(PairedTagsCheckerRunTest, RunInsideNotExtendedCsvIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[csv header]a;\n[run]x[/run];[/csv]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].line, 2);
    EXPECT_TRUE(anyErrorContains(errors, "[csv ext]"));
}

TEST(PairedTagsCheckerRunTest, InnermostContainerDecidesAboutExt)
{
    // [pkt ext] is fine, but [run] belongs to the inner [csv] which is not extended
    const auto errors = PairedTagsChecker::checkTags("[pkt ext][run][csv]a;\n[run]x[/run];[/csv][/run][/pkt]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].line, 2);
    EXPECT_TRUE(anyErrorContains(errors, "[csv ext]"));
}

TEST(PairedTagsCheckerRunTest, RunInsideCodeBlockIsNotInterpreted)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[code]\n[run]\n[/code]").empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Legal tags
// ---------------------------------------------------------------------------------------------------------------------

TEST(PairedTagsCheckerUnknownTagTest, UnknownTagIsReportedAndNotTreatedAsUnclosed)
{
    const auto errors = PairedTagsChecker::checkTags("array[idx] is not a tag");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].line, 1);
    EXPECT_EQ(errors[0].positionInLine, 5);
    EXPECT_TRUE(anyErrorContains(errors, "unknown tag"));
}

TEST(PairedTagsCheckerUnknownTagTest, UnknownOpeningAndClosingTagsAreBothReported)
{
    const auto errors = PairedTagsChecker::checkTags("[foo]text[/foo]");
    EXPECT_EQ(errors.size(), 2u);
}

TEST(PairedTagsCheckerUnknownTagTest, UnknownTagInsideCodeBlocksIsIgnored)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[code]a[idx]b[/code]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[cpp]v[i] = t[j];[/cpp]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[py]a = b[key][/py]").empty());
}

TEST(PairedTagsCheckerUnknownTagTest, TagNamesAreCaseSensitive)
{
    EXPECT_EQ(PairedTagsChecker::checkTags("[B]x[/B]").size(), 2u);
}

TEST(PairedTagsCheckerUnknownTagTest, AllKnownTagsAreAccepted)
{
    const std::string text =
        "[b]a[/b][i]a[/i][u]a[/u][s]a[/s][tt]a[/tt][sub]a[/sub][sup]a[/sup]\n"
        "[h1]a[/h1][h2]a[/h2][h3]a[/h3][h4]a[/h4][h5]a[/h5][h6]a[/h6]\n"
        "[cytat]a[/cytat][div]a[/div][div class=\"tip\"]a[/div][pkt]a[/pkt][csv]a;[/csv][log]a[/log]\n"
        "[google]phrase[/google][g]phrase[/g]\n"
        "[a href=\"https://cpp0x.pl\"][img src=\"a.png\"]\n"
        "[doc id=1][tut id=2][art id=3][topic id=\"x\"][t id=\"x\"]\n"
        "[doc_rel list=\"1,2\"][tut_rel list=\"1\" header][art_rel list=\"1\"]";
    const auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty()) << errors.front().line << ": " << errors.front().errorText;
}

// ---------------------------------------------------------------------------------------------------------------------
// Closing tags
// ---------------------------------------------------------------------------------------------------------------------

TEST(PairedTagsCheckerClosingTest, ClosingNeverOpenedTagIsDistinguishedFromMismatch)
{
    const auto neverOpened = PairedTagsChecker::checkTags("[b]x[/i]");
    EXPECT_TRUE(anyErrorContains(neverOpened, "never opened"));

    const auto mismatch = PairedTagsChecker::checkTags("[b][i]x[/b][/i]");
    EXPECT_FALSE(anyErrorContains(mismatch, "never opened"));
    EXPECT_FALSE(mismatch.empty());
}

TEST(PairedTagsCheckerClosingTest, OneSidedTagsMustNotBeClosed)
{
    const auto errors = PairedTagsChecker::checkTags("[a href=\"x.pl\"]link[/a] and [img src=\"a.png\"][/img]");
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_TRUE(anyErrorContains(errors, "one-sided"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------------------------------------------------

TEST(PairedTagsCheckerAttributesTest, CorrectAttributesAreAccepted)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[a href=\"https://x.pl\" name=\"Some name\"]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[img src=\"a.png\" alt=\"\" opis=\"d\" autofit]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[img autofit alt=\"x\" src=\"a b.png\"]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[code src=\"C++\"]int a;[/code]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[pkt ext]a[/pkt][csv ext header]a;[/csv]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[doc id=123 name=\"x\"][doc id=\"123\"]").empty());
}

TEST(PairedTagsCheckerAttributesTest, UnknownAttributeIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[a href=\"x.pl\" color=\"red\"]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(anyErrorContains(errors, "color"));
    EXPECT_TRUE(anyErrorContains(errors, "href, name"));
}

TEST(PairedTagsCheckerAttributesTest, TagWithoutAttributesReportsAnyAttribute)
{
    const auto errors = PairedTagsChecker::checkTags("[b class=\"x\"]a[/b]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(anyErrorContains(errors, "takes no attributes"));
}

TEST(PairedTagsCheckerAttributesTest, ValueWithoutQuotesIsReported)
{
    const auto bare = PairedTagsChecker::checkTags("[a href=https://x.pl]");
    ASSERT_EQ(bare.size(), 1u);
    EXPECT_TRUE(anyErrorContains(bare, "double quotes"));

    const auto single = PairedTagsChecker::checkTags("[img src='a.png']");
    ASSERT_EQ(single.size(), 1u);
    EXPECT_TRUE(anyErrorContains(single, "double quotes"));
}

TEST(PairedTagsCheckerAttributesTest, AttributeWithoutValueIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[a href]");
    EXPECT_TRUE(anyErrorContains(errors, "needs a value"));
    EXPECT_EQ(PairedTagsChecker::checkTags("[img src=]").size(), 1u);
}

TEST(PairedTagsCheckerAttributesTest, UnterminatedQuoteIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[a href=\"https://x.pl]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(anyErrorContains(errors, "closing quotation mark"));
}

TEST(PairedTagsCheckerAttributesTest, EmptyAttributeIsReportedExceptWhereItIsLegal)
{
    EXPECT_TRUE(anyErrorContains(PairedTagsChecker::checkTags("[a href=\"\"]"), "is empty"));
    EXPECT_TRUE(anyErrorContains(PairedTagsChecker::checkTags("[img src=\"\"]"), "is empty"));
    EXPECT_TRUE(PairedTagsChecker::checkTags("[img src=\"a.png\" alt=\"\"]").empty()); // empty alt = decorative image
}

TEST(PairedTagsCheckerAttributesTest, FlagWithValueIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[pkt ext=\"1\"]a[/pkt]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(anyErrorContains(errors, "flag"));
}

TEST(PairedTagsCheckerAttributesTest, MissingRequiredAttributeIsReported)
{
    EXPECT_TRUE(anyErrorContains(PairedTagsChecker::checkTags("[a name=\"abc\"]"), "needs the attribute href"));
    EXPECT_TRUE(anyErrorContains(PairedTagsChecker::checkTags("[doc name=\"abc\"]"), "needs the attribute id"));
    EXPECT_TRUE(anyErrorContains(PairedTagsChecker::checkTags("[doc_rel header]"), "needs the attribute list"));
    EXPECT_TRUE(PairedTagsChecker::checkTags("[img]").empty()); // [img] without any attribute stays accepted
}

TEST(PairedTagsCheckerAttributesTest, DuplicatedAttributeIsReported)
{
    const auto errors = PairedTagsChecker::checkTags("[a href=\"x.pl\" href=\"y.pl\"]");
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(anyErrorContains(errors, "twice"));
}

TEST(PairedTagsCheckerAttributesTest, NumericIdMustBeNumberOrQuoted)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[doc id=274]").empty());
    EXPECT_TRUE(PairedTagsChecker::checkTags("[doc id=\"274\"]").empty());
    EXPECT_EQ(PairedTagsChecker::checkTags("[doc id=abc]").size(), 1u);
    EXPECT_EQ(PairedTagsChecker::checkTags("[topic id=abc]").size(), 1u); // [topic] needs quotes
}

TEST(PairedTagsCheckerAttributesTest, AttributesInsideCodeBlocksAreNotChecked)
{
    EXPECT_TRUE(PairedTagsChecker::checkTags("[code]\n[a href=broken]\n[/code]").empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// Positions
// ---------------------------------------------------------------------------------------------------------------------

TEST(PairedTagsCheckerPositionTest, PositionIsCountedFromTheBeginningOfTheLine)
{
    const auto errors = PairedTagsChecker::checkTags("ab [foo] cd [bar]");
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0].positionInLine, 3);
    EXPECT_EQ(errors[1].positionInLine, 12);
}
