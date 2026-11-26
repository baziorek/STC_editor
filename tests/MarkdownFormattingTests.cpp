#include <gtest/gtest.h>

#include <QApplication>
#include <QPlainTextEdit>
#include <QTextCursor>

#include "utils/MarkdownFormatting.h"

namespace
{

class MarkdownFormattingTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        editor.setPlainText(QString());
    }

    /// Sets the text and selects [start, end) (end < 0 selects nothing, the cursor goes to start).
    void setTextAndSelection(const QString& text, int start, int end = -1)
    {
        editor.setPlainText(text);
        QTextCursor cursor = editor.textCursor();
        cursor.setPosition(start);
        if (end >= 0)
            cursor.setPosition(end, QTextCursor::KeepAnchor);
        editor.setTextCursor(cursor);
    }

    QPlainTextEdit editor;
};

TEST_F(MarkdownFormattingTests, BoldWrapsSelection)
{
    setTextAndSelection(QStringLiteral("hello world"), 0, 5);
    markdownedit::toggleInline(&editor, QStringLiteral("**"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("**hello** world"));
}

TEST_F(MarkdownFormattingTests, BoldUnwrapsSelection)
{
    setTextAndSelection(QStringLiteral("**hello** world"), 2, 7);
    markdownedit::toggleInline(&editor, QStringLiteral("**"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("hello world"));
}

TEST_F(MarkdownFormattingTests, BoldUnwrapsWhenWholeWrappedTextSelected)
{
    setTextAndSelection(QStringLiteral("say **hi** ok"), 4, 10);
    markdownedit::toggleInline(&editor, QStringLiteral("**"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("say hi ok"));
}

TEST_F(MarkdownFormattingTests, ItalicDoesNotConfuseBold)
{
    // "**x**" must not count as italic-wrapped: toggling "*" wraps it into bold-italic
    setTextAndSelection(QStringLiteral("**x**"), 0, 5);
    markdownedit::toggleInline(&editor, QStringLiteral("*"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("***x***"));
}

TEST_F(MarkdownFormattingTests, ItalicInsideBoldSelection)
{
    // the selection sits inside "**": the single "*" check must not strip the bold markers
    setTextAndSelection(QStringLiteral("**bold**"), 2, 6);
    markdownedit::toggleInline(&editor, QStringLiteral("*"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("***bold***"));
}

TEST_F(MarkdownFormattingTests, InlineCodeEmptySelection)
{
    setTextAndSelection(QStringLiteral("abc"), 1);
    markdownedit::toggleInline(&editor, QStringLiteral("`"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("a``bc"));
    // the cursor is between the backticks
    EXPECT_EQ(editor.textCursor().position(), 2);
}

TEST_F(MarkdownFormattingTests, StrikeWraps)
{
    setTextAndSelection(QStringLiteral("old"), 0, 3);
    markdownedit::toggleInline(&editor, QStringLiteral("~~"));
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("~~old~~"));
}

TEST_F(MarkdownFormattingTests, HeadingAddedAndRemoved)
{
    setTextAndSelection(QStringLiteral("Title"), 0);
    markdownedit::toggleHeading(&editor, 1);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("# Title"));

    markdownedit::toggleHeading(&editor, 1);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("Title"));
}

TEST_F(MarkdownFormattingTests, HeadingChangesLevel)
{
    setTextAndSelection(QStringLiteral("## Sub"), 0);
    markdownedit::toggleHeading(&editor, 3);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("### Sub"));
}

TEST_F(MarkdownFormattingTests, HeadingAppliesToSelectedLines)
{
    setTextAndSelection(QStringLiteral("one\ntwo\nthree"), 0, 7);
    markdownedit::toggleHeading(&editor, 2);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("## one\n## two\nthree"));
}

TEST_F(MarkdownFormattingTests, QuoteToggles)
{
    setTextAndSelection(QStringLiteral("quoted"), 0);
    markdownedit::toggleQuote(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("> quoted"));

    markdownedit::toggleQuote(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("quoted"));
}

TEST_F(MarkdownFormattingTests, QuoteNestedRemovesOneLevel)
{
    setTextAndSelection(QStringLiteral("> > deep"), 0);
    markdownedit::toggleQuote(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("> deep"));
}

TEST_F(MarkdownFormattingTests, LinkFromUrlSelection)
{
    // the words around the URL inside the selection become the link text
    const QString text = QStringLiteral("see https://example.com here");
    setTextAndSelection(text, 0, text.size());
    markdownedit::insertLink(&editor);
    EXPECT_EQ(editor.toPlainText(),
              QStringLiteral("[see here](https://example.com)"));
}

TEST_F(MarkdownFormattingTests, LinkFromUrlOnlySelection)
{
    const QString text = QStringLiteral("see https://example.com here");
    setTextAndSelection(text, 4, 23);
    markdownedit::insertLink(&editor);
    EXPECT_EQ(editor.toPlainText(),
              QStringLiteral("see [https://example.com](https://example.com) here"));
}

TEST_F(MarkdownFormattingTests, LinkFromPlainSelection)
{
    setTextAndSelection(QStringLiteral("click me"), 0, 8);
    markdownedit::insertLink(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("[click me]()"));
}

TEST_F(MarkdownFormattingTests, LinkEmptySelection)
{
    setTextAndSelection(QStringLiteral("ab"), 1);
    markdownedit::insertLink(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("a[]()b"));
    EXPECT_EQ(editor.textCursor().position(), 4); // inside the parentheses
}

TEST_F(MarkdownFormattingTests, ImageFromPathSelection)
{
    setTextAndSelection(QStringLiteral("pic.png"), 0, 7);
    markdownedit::insertImage(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("![](pic.png)"));
}

TEST_F(MarkdownFormattingTests, ImageFromDescription)
{
    setTextAndSelection(QStringLiteral("a cat"), 0, 5);
    markdownedit::insertImage(&editor);
    EXPECT_EQ(editor.toPlainText(), QStringLiteral("![a cat]()"));
}

TEST_F(MarkdownFormattingTests, CodeBlockEmpty)
{
    setTextAndSelection(QStringLiteral("x = 1;"), 0);
    markdownedit::insertCodeBlock(&editor, QStringLiteral("cpp"));
    EXPECT_EQ(editor.toPlainText(),
              QStringLiteral("```cpp\n\n```\nx = 1;"));
    // the cursor is on the empty line inside the fence
    EXPECT_EQ(editor.toPlainText().mid(editor.textCursor().position(), 1), QStringLiteral("\n"));
}

TEST_F(MarkdownFormattingTests, CodeBlockWrapsSelection)
{
    setTextAndSelection(QStringLiteral("int a;\nint b;\nend"), 0, 14);
    markdownedit::insertCodeBlock(&editor, QStringLiteral("python"));
    EXPECT_EQ(editor.toPlainText(),
              QStringLiteral("```python\nint a;\nint b;\n```\nend"));
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("STC_editor_tests");
    QCoreApplication::setApplicationName("markdown_formatting_tests");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
