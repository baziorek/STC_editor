// CodeBlock: which program handles a block in the compiler window. (main() is in EditHistoryTests.cpp.)
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
    for (const CodeBlock& b : { block("cpp"), block("code", "c++"), block("code", "cpp") })
    {
        ASSERT_TRUE(b.runnableLanguage().has_value()) << b.tag.toStdString() << " " << b.language.toStdString();
        EXPECT_EQ(*b.runnableLanguage(), RunnableLanguage::Cpp);
        EXPECT_TRUE(b.canBeCompiled());
    }
}

TEST(CodeBlock, PythonBlocksAreCheckedWithPython)
{
    for (const CodeBlock& b : { block("py"), block("code", "python"), block("code", "py") })
    {
        ASSERT_TRUE(b.runnableLanguage().has_value()) << b.tag.toStdString() << " " << b.language.toStdString();
        EXPECT_EQ(*b.runnableLanguage(), RunnableLanguage::Python);
    }
}

TEST(CodeBlock, PlainCodeBlocksAreCheckedWithBashNotCompiledAsCpp)
{
    // [code] without src= is a console session, not C++
    for (const CodeBlock& b : { block("code"), block("code", "bash"), block("code", "sh"), block("code", "shell") })
    {
        ASSERT_TRUE(b.runnableLanguage().has_value()) << b.tag.toStdString() << " " << b.language.toStdString();
        EXPECT_EQ(*b.runnableLanguage(), RunnableLanguage::Bash);
        EXPECT_FALSE(b.isCpp());
    }
}

TEST(CodeBlock, OtherLanguagesCannotBeRun)
{
    for (const CodeBlock& b : { block("code", "xml"), block("code", "json"), block("log") })
        EXPECT_FALSE(b.canBeCompiled()) << b.tag.toStdString() << " " << b.language.toStdString();
}
