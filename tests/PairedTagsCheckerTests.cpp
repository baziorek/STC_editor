// Tests generated with ChatGPT
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

// Test that tags inside [run] are ignored
TEST(PairedTagsCheckerTest, IgnoreTagsInsideRun)
{
    std::string text = "Normal text [b]bold[/b].\n"
                       "[run] This text can contain [i] other tags [/i] and still work [/run].\n"
                       "[i]Italic[/i] after run.";
    auto errors = PairedTagsChecker::checkTags(text);
    EXPECT_TRUE(errors.empty()) << "size: " << errors.size();
}

// Test that unclosed tags inside [run] are correctly reported
TEST(PairedTagsCheckerTest, ReportUnclosedTagsInsideRun)
{
    std::string text = "[run] Unclosed [b]tag inside run[/run].";
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
