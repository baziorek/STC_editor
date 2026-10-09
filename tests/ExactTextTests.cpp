// exactPlainText(): the text of a document as it is saved to a file. (main() is in EditHistoryTests.cpp.)
#include <gtest/gtest.h>

#include <QPlainTextEdit>
#include <QTextBlock>

#include "utils/ExactText.h"

namespace
{
const QString nonBreakingSpaceAndLineSeparator = QStringLiteral("a b c\nd");
} // namespace

TEST(ExactText, KeepsNonBreakingSpacesAndLineSeparators)
{
    QPlainTextEdit edit;
    edit.setPlainText(nonBreakingSpaceAndLineSeparator);

    EXPECT_EQ(exactPlainText(edit.document()), nonBreakingSpaceAndLineSeparator);
    // This is why exactPlainText() exists: toPlainText() changes the text
    EXPECT_NE(edit.toPlainText(), nonBreakingSpaceAndLineSeparator);
    EXPECT_TRUE(edit.toPlainText().contains(QLatin1String("a b")));
}

TEST(ExactText, HasOneLinePerBlockEvenWithLineSeparators)
{
    QPlainTextEdit edit;
    edit.setPlainText(nonBreakingSpaceAndLineSeparator);

    // The numbers of the lines shown in the editor are the numbers of blocks: the text has to agree with them
    EXPECT_EQ(exactPlainText(edit.document()).split(QLatin1Char('\n')).size(), edit.document()->blockCount());
    EXPECT_NE(edit.toPlainText().split(QLatin1Char('\n')).size(), edit.document()->blockCount());
}

TEST(ExactText, IsTheSameAsPlainTextForAnOrdinaryText)
{
    for (const QString& text : { QString(), QStringLiteral("\n\n"), QStringLiteral("plain\ntext\n"),
                                 QStringLiteral("tab\there\nżółć gęślą\n"), QStringLiteral("no end of line") })
    {
        QPlainTextEdit edit;
        edit.setPlainText(text);
        EXPECT_EQ(exactPlainText(edit.document()), text);
        EXPECT_EQ(exactPlainText(edit.document()), edit.toPlainText());
    }
}

TEST(ExactText, FollowsEditsAndLeavesNoParagraphSeparators)
{
    QPlainTextEdit edit;
    edit.setPlainText(QStringLiteral("one\ntwo"));
    QTextCursor cursor(edit.document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral(" three\nfour"));

    const QString text = exactPlainText(edit.document());
    EXPECT_EQ(text, QStringLiteral("one\ntwo three\nfour"));
    EXPECT_FALSE(text.contains(QChar::ParagraphSeparator));
}
