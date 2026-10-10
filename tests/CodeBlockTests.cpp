// CodeBlock: which compiler window handles a block. (main() is in EditHistoryTests.cpp.)
#include <gtest/gtest.h>

#include "types/CodeBlock.h"

namespace
{
CodeBlock block(const QString& tag, const QString& language = QString())
{
    CodeBlock b;
    b.tag = tag;
    b.language = language;
    return b;
}
} // namespace

TEST(CodeBlock, CppBlocksAreCompiledWithGpp)
{
    for (const CodeBlock& b : { block("cpp"), block("code"), block("code", "c++"), block("code", "cpp") })
    {
        EXPECT_TRUE(b.isCpp()) << b.tag.toStdString() << " " << b.language.toStdString();
        EXPECT_FALSE(b.isPython());
        EXPECT_TRUE(b.canBeCompiled());
    }
}

TEST(CodeBlock, PythonBlocksAreCheckedWithPython)
{
    for (const CodeBlock& b : { block("py"), block("code", "python"), block("code", "py") })
    {
        EXPECT_TRUE(b.isPython()) << b.tag.toStdString() << " " << b.language.toStdString();
        EXPECT_FALSE(b.isCpp());
        EXPECT_TRUE(b.canBeCompiled());
    }
}

TEST(CodeBlock, OtherLanguagesCannotBeCompiled)
{
    for (const CodeBlock& b : { block("code", "bash"), block("code", "xml"), block("log") })
        EXPECT_FALSE(b.canBeCompiled()) << b.tag.toStdString() << " " << b.language.toStdString();
}
