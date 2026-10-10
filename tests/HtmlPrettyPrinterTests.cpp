#include <gtest/gtest.h>
#include <QFile>
#include <cstdlib>
#include "utils/HtmlPrettyPrinter.h"

using HtmlPrettyPrinter::prettyPrint;

namespace
{
QString withoutWhitespace(const QString& text)
{
    QString result;
    for (const QChar c : text)
    {
        if (!c.isSpace())
            result += c;
    }
    return result;
}
} // namespace

TEST(HtmlPrettyPrinter, BlockElementsAreIndentedByNesting)
{
    EXPECT_EQ(prettyPrint("<div><table><tr><td>a</td><td>b</td></tr></table></div>"),
              "<div>\n"
              "  <table>\n"
              "    <tr>\n"
              "      <td>\n"
              "        a\n"
              "      </td>\n"
              "      <td>\n"
              "        b\n"
              "      </td>\n"
              "    </tr>\n"
              "  </table>\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, InlineElementsStayInTheLine)
{
    EXPECT_EQ(prettyPrint("<div>Text <b>bold</b> and <span class=\"x\">span</span>.</div>"),
              "<div>\n"
              "  Text <b>bold</b> and <span class=\"x\">span</span>.\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, LineBreakEndsTheLine)
{
    EXPECT_EQ(prettyPrint("<div class=\"codeCpp\"><span>int </span>main()<br>{<br>}<br></div>"),
              "<div class=\"codeCpp\">\n"
              "  <span>int </span>main()<br>\n"
              "  {<br>\n"
              "  }<br>\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, VoidElementsDoNotOpenALevel)
{
    EXPECT_EQ(prettyPrint("<div><img src=\"a.png\"><hr><p>x</p></div>"),
              "<div>\n"
              "  <img src=\"a.png\">\n"
              "  <hr>\n"
              "  <p>\n"
              "    x\n"
              "  </p>\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, GreaterThanInAnAttributeDoesNotEndTheTag)
{
    EXPECT_EQ(prettyPrint("<div><a title=\"a > b\" href='x>y'>link</a></div>"),
              "<div>\n"
              "  <a title=\"a > b\" href='x>y'>link</a>\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, LessThanInATextIsNotATag)
{
    EXPECT_EQ(prettyPrint("<div>a < b and c <= d</div>"),
              "<div>\n"
              "  a < b and c <= d\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, CommentIsInItsOwnLine)
{
    EXPECT_EQ(prettyPrint("<div>a<!-- <b>not a tag</b> -->b</div>"),
              "<div>\n"
              "  a\n"
              "  <!-- <b>not a tag</b> -->\n"
              "  b\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, PreIsNotTouched)
{
    const QString pre = "<pre>  int  a;\n    if (a &lt; 1) <b>x</b>\n</pre>";
    EXPECT_EQ(prettyPrint("<div>" + pre + "</div>"), "<div>\n  " + pre + "\n</div>\n");
}

TEST(HtmlPrettyPrinter, WhitespaceIsOneSpace)
{
    EXPECT_EQ(prettyPrint("<div>a \n\n   b\t c&nbsp;&nbsp;d</div>"),
              "<div>\n"
              "  a b c&nbsp;&nbsp;d\n"
              "</div>\n");
}

TEST(HtmlPrettyPrinter, ClosingTagWithoutOpeningDoesNotBreakTheIndent)
{
    const QString result = prettyPrint("</div></td><p>x</p>");
    EXPECT_TRUE(result.startsWith("</div>\n</td>\n<p>\n  x\n</p>")) << result.toStdString();
}

TEST(HtmlPrettyPrinter, NotClosedTagIsKeptAsText)
{
    const QString html = "<div>text <span class=\"a";
    EXPECT_EQ(withoutWhitespace(prettyPrint(html)), withoutWhitespace(html));
}

TEST(HtmlPrettyPrinter, EmptyInput)
{
    EXPECT_EQ(prettyPrint(""), "");
}

TEST(HtmlPrettyPrinter, SecondPassChangesNothing)
{
    const QString html = "<table class=\"FormatCSV\"><tr><th>C++</th></tr><tr><td><div class=\"codeInfo\"><div class=\"header\">C/C++</div>"
                         "<div class=\"codeCpp\"><span class=\"keyword\">int </span>main<span>()<br></span><span>{<br>&nbsp; &nbsp; </span>"
                         "return 0<span>;<br>}<br></span></div></div></td></tr></table><br><br><h3>Title</h3>text <b>b</b>";
    const QString once = prettyPrint(html);
    EXPECT_EQ(prettyPrint(once), once);
}

TEST(HtmlPrettyPrinter, NothingIsLostInARealPage)
{
    // a file with the HTML which cpp0x.pl returned, when there is one at hand
    const char* path = std::getenv("STC_TEST_HTML_FIXTURE");
    if (!path)
        GTEST_SKIP() << "STC_TEST_HTML_FIXTURE is not set";

    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(file.readAll());

    const QString pretty = prettyPrint(html);

    EXPECT_EQ(withoutWhitespace(pretty), withoutWhitespace(html)) << "only the layout may change";
    EXPECT_GT(pretty.count('\n'), 1000);
    EXPECT_EQ(prettyPrint(pretty), pretty);
}
