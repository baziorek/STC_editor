#include <gtest/gtest.h>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include "utils/ConsoleShell.h"

using ConsoleShell::Builtin;
using ConsoleShell::BuiltinKind;
using ConsoleShell::CommandKind;
using ConsoleShell::Token;
using ConsoleShell::TokenKind;

namespace
{
/// "kind:text" of every token, to compare a whole line at once
QStringList describe(const QString& line)
{
    static const char* const names[] = {"cmd", "assign", "arg", "opt", "str", "var", "op", "comment"};
    QStringList result;
    for (const Token& token : ConsoleShell::tokenize(line))
        result << QStringLiteral("%1:%2").arg(QLatin1StringView(names[static_cast<int>(token.kind)]), line.mid(token.start, token.length));
    return result;
}

QProcessEnvironment environmentWith(const QString& name, const QString& value)
{
    QProcessEnvironment environment;
    environment.insert(name, value);
    return environment;
}
} // namespace

// ------------------------------------------------------------------ tokenizer

TEST(ConsoleShellTokenize, EmptyAndWhiteSpaceLinesHaveNoTokens)
{
    EXPECT_TRUE(ConsoleShell::tokenize("").isEmpty());
    EXPECT_TRUE(ConsoleShell::tokenize("   \t ").isEmpty());
}

TEST(ConsoleShellTokenize, CommandOptionsAndArguments)
{
    EXPECT_EQ(describe("g++ -std=c++23 -O2 main.cpp"),
              (QStringList{"cmd:g++", "opt:-std=c++23", "opt:-O2", "arg:main.cpp"}));
    EXPECT_EQ(describe("  ls   --all  "), (QStringList{"cmd:ls", "opt:--all"}));
}

TEST(ConsoleShellTokenize, SingleDashIsAnArgument)
{
    EXPECT_EQ(describe("cd -"), (QStringList{"cmd:cd", "arg:-"}));
}

TEST(ConsoleShellTokenize, QuotedStrings)
{
    EXPECT_EQ(describe(R"(echo "a b" 'c d')"), (QStringList{"cmd:echo", R"(str:"a b")", "str:'c d'"}));
}

TEST(ConsoleShellTokenize, UnfinishedStringGoesToTheEndOfTheLine)
{
    EXPECT_EQ(describe(R"(echo "abc def)"), (QStringList{"cmd:echo", R"(str:"abc def)"}));
    EXPECT_EQ(describe("echo 'abc"), (QStringList{"cmd:echo", "str:'abc"}));
}

TEST(ConsoleShellTokenize, QuoteInsideAWordDoesNotSplitIt)
{
    EXPECT_EQ(describe(R"(g++ --name="a b" x)"), (QStringList{"cmd:g++", R"(opt:--name="a b")", "arg:x"}));
}

#ifndef Q_OS_WIN
TEST(ConsoleShellTokenize, EscapedQuoteDoesNotCloseTheString)
{
    EXPECT_EQ(describe(R"(echo "a \" b" x)"), (QStringList{"cmd:echo", R"(str:"a \" b")", "arg:x"}));
}

TEST(ConsoleShellTokenize, EscapedSpaceKeepsTheWordTogether)
{
    EXPECT_EQ(describe(R"(cd my\ dir)"), (QStringList{"cmd:cd", R"(arg:my\ dir)"}));
}
#endif

TEST(ConsoleShellTokenize, Variables)
{
    EXPECT_EQ(describe("echo $HOME ${USER} $? $"), (QStringList{"cmd:echo", "var:$HOME", "var:${USER}", "var:$?", "arg:$"}));
}

TEST(ConsoleShellTokenize, VariableEndsAWord)
{
    EXPECT_EQ(describe("ls dir/$NAME/x"), (QStringList{"cmd:ls", "arg:dir/", "var:$NAME", "arg:/x"}));
}

TEST(ConsoleShellTokenize, UnfinishedBracedVariableGoesToTheEnd)
{
    EXPECT_EQ(describe("echo ${HO"), (QStringList{"cmd:echo", "var:${HO"}));
}

TEST(ConsoleShellTokenize, OperatorsStartANewCommand)
{
    EXPECT_EQ(describe("ls | grep x && echo ok; pwd"),
              (QStringList{"cmd:ls", "op:|", "cmd:grep", "arg:x", "op:&&", "cmd:echo", "arg:ok", "op:;", "cmd:pwd"}));
}

TEST(ConsoleShellTokenize, OperatorsNeedNoSpaces)
{
    EXPECT_EQ(describe("a&&b|c"), (QStringList{"cmd:a", "op:&&", "cmd:b", "op:|", "cmd:c"}));
}

TEST(ConsoleShellTokenize, RedirectionIsFollowedByAFileNotACommand)
{
    EXPECT_EQ(describe("make > out.txt"), (QStringList{"cmd:make", "op:>", "arg:out.txt"}));
    EXPECT_EQ(describe("make 2>err.txt"), (QStringList{"cmd:make", "op:2>", "arg:err.txt"}));
    EXPECT_EQ(describe("make 2>&1 | less"), (QStringList{"cmd:make", "op:2>&", "arg:1", "op:|", "cmd:less"}));
}

TEST(ConsoleShellTokenize, NumberWhichIsNotARedirectionIsAnArgument)
{
    EXPECT_EQ(describe("sleep 10"), (QStringList{"cmd:sleep", "arg:10"}));
}

TEST(ConsoleShellTokenize, SubshellAndCommandSubstitution)
{
    EXPECT_EQ(describe("(cd x && ls)"), (QStringList{"op:(", "cmd:cd", "arg:x", "op:&&", "cmd:ls", "op:)"}));
    EXPECT_EQ(describe("echo $(date)"), (QStringList{"cmd:echo", "op:$(", "cmd:date", "op:)"}));
}

TEST(ConsoleShellTokenize, AssignmentsInFrontOfTheCommand)
{
    EXPECT_EQ(describe("CXX=g++ make -j8"), (QStringList{"assign:CXX=g++", "cmd:make", "opt:-j8"}));
    EXPECT_EQ(describe("a=b"), (QStringList{"assign:a=b"}));
}

TEST(ConsoleShellTokenize, EqualsSignInAnArgumentIsNotAnAssignment)
{
    EXPECT_EQ(describe("make CXX=g++"), (QStringList{"cmd:make", "arg:CXX=g++"}));
    EXPECT_EQ(describe("1a=b"), (QStringList{"cmd:1a=b"}));
}

TEST(ConsoleShellTokenize, CommentGoesToTheEndOfTheLine)
{
    EXPECT_EQ(describe("ls # all files; really"), (QStringList{"cmd:ls", "comment:# all files; really"}));
    EXPECT_EQ(describe("# only a comment"), (QStringList{"comment:# only a comment"}));
}

TEST(ConsoleShellTokenize, HashInTheMiddleOfAWordIsNotAComment)
{
    EXPECT_EQ(describe("echo a#b"), (QStringList{"cmd:echo", "arg:a#b"}));
}

TEST(ConsoleShellTokenize, WordsAfterPrefixCommandsAreCommandsToo)
{
    EXPECT_EQ(describe("sudo make install"), (QStringList{"cmd:sudo", "cmd:make", "arg:install"}));
    EXPECT_EQ(describe("if test -f x; then echo y; fi"),
              (QStringList{"cmd:if", "cmd:test", "opt:-f", "arg:x", "op:;", "cmd:then", "cmd:echo", "arg:y", "op:;", "cmd:fi"}));
}

TEST(ConsoleShellTokenize, TokensAreSortedAndDoNotOverlap)
{
    const QString line = R"(A=1 ls -l "x y" $HOME/a | wc -l >out 2>&1 # end)";
    qsizetype end = 0;
    for (const Token& token : ConsoleShell::tokenize(line))
    {
        EXPECT_GE(token.start, end);
        EXPECT_GT(token.length, 0);
        end = token.start + token.length;
        EXPECT_LE(end, line.size());
    }
}

// ------------------------------------------------------------------ words

TEST(ConsoleShellSplitWords, PlainWords)
{
    EXPECT_EQ(ConsoleShell::splitWords("  cd   dir "), (QStringList{"cd", "dir"}));
    EXPECT_TRUE(ConsoleShell::splitWords("   ").isEmpty());
}

TEST(ConsoleShellSplitWords, QuotesAreRemoved)
{
    EXPECT_EQ(ConsoleShell::splitWords(R"(cd "my dir")"), (QStringList{"cd", "my dir"}));
    EXPECT_EQ(ConsoleShell::splitWords("cd 'my dir'"), (QStringList{"cd", "my dir"}));
    EXPECT_EQ(ConsoleShell::splitWords(R"(a"b c"d)"), (QStringList{"ab cd"}));
}

TEST(ConsoleShellSplitWords, EmptyQuotesAreAnEmptyWord)
{
    EXPECT_EQ(ConsoleShell::splitWords(R"(cd "")"), (QStringList{"cd", ""}));
}

#ifndef Q_OS_WIN
TEST(ConsoleShellSplitWords, BackslashEscapes)
{
    EXPECT_EQ(ConsoleShell::splitWords(R"(cd my\ dir)"), (QStringList{"cd", "my dir"}));
    EXPECT_EQ(ConsoleShell::splitWords(R"(echo "a\"b" 'c\d')"), (QStringList{"echo", R"(a"b)", R"(c\d)"}));
    EXPECT_EQ(ConsoleShell::splitWords(R"(echo "a\nb")"), (QStringList{"echo", R"(a\nb)"}));
}
#endif

TEST(ConsoleShellSplitWords, VariablesAreExpandedOnlyWhenAskedAndNotInSingleQuotes)
{
    const QProcessEnvironment environment = environmentWith("DIR", "/tmp/x");
    EXPECT_EQ(ConsoleShell::splitWords("cd $DIR/sub", &environment), (QStringList{"cd", "/tmp/x/sub"}));
    EXPECT_EQ(ConsoleShell::splitWords("cd ${DIR}", &environment), (QStringList{"cd", "/tmp/x"}));
    EXPECT_EQ(ConsoleShell::splitWords(R"(cd "$DIR")", &environment), (QStringList{"cd", "/tmp/x"}));
    EXPECT_EQ(ConsoleShell::splitWords("cd '$DIR'", &environment), (QStringList{"cd", "$DIR"}));
    EXPECT_EQ(ConsoleShell::splitWords("cd $NOPE", &environment), (QStringList{"cd", ""}));
    EXPECT_EQ(ConsoleShell::splitWords("cd $DIR"), (QStringList{"cd", "$DIR"}));
}

// ------------------------------------------------------------------ cd

TEST(ConsoleShellCd, EmptyAndTildeLeadHome)
{
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "", "/home/u", "/p"), "/home/u");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "~", "/home/u", "/p"), "/home/u");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "~/src", "/home/u", "/p"), "/home/u/src");
}

TEST(ConsoleShellCd, DashLeadsToThePreviousDirectory)
{
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "-", "/home/u", "/p"), "/p");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "-", "/home/u", ""), "");
}

TEST(ConsoleShellCd, RelativeAndAbsolutePaths)
{
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "c", "/h", ""), "/a/b/c");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "..", "/h", ""), "/a");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "../x/./y/", "/h", ""), "/a/x/y");
    EXPECT_EQ(ConsoleShell::resolveCdTarget("/a/b", "/etc/", "/h", ""), "/etc");
}

// ------------------------------------------------------------------ built-ins

TEST(ConsoleShellBuiltin, Cd)
{
    const QProcessEnvironment environment = environmentWith("STC_DIR", "/work");
    Builtin builtin = ConsoleShell::parseBuiltin("cd", environment);
    EXPECT_EQ(builtin.kind, BuiltinKind::ChangeDirectory);
    EXPECT_EQ(builtin.argument, "");

    builtin = ConsoleShell::parseBuiltin("  cd   \"my dir\"  ", environment);
    EXPECT_EQ(builtin.kind, BuiltinKind::ChangeDirectory);
    EXPECT_EQ(builtin.argument, "my dir");

    builtin = ConsoleShell::parseBuiltin("cd $STC_DIR", environment);
    EXPECT_EQ(builtin.kind, BuiltinKind::ChangeDirectory);
    EXPECT_EQ(builtin.argument, "/work");

    builtin = ConsoleShell::parseBuiltin("cd .. # up", environment);
    EXPECT_EQ(builtin.kind, BuiltinKind::ChangeDirectory);
    EXPECT_EQ(builtin.argument, "..");
}

TEST(ConsoleShellBuiltin, Clear)
{
    EXPECT_EQ(ConsoleShell::parseBuiltin("clear", {}).kind, BuiltinKind::Clear);
    EXPECT_EQ(ConsoleShell::parseBuiltin(" cls ", {}).kind, BuiltinKind::Clear);
    EXPECT_EQ(ConsoleShell::parseBuiltin("clear -x", {}).kind, BuiltinKind::None);
}

TEST(ConsoleShellBuiltin, EverythingElseIsLeftToTheShell)
{
    EXPECT_EQ(ConsoleShell::parseBuiltin("", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("ls", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("cd a b", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("cd build && make", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("cd x | cat", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("X=1 cd x", {}).kind, BuiltinKind::None);
    EXPECT_EQ(ConsoleShell::parseBuiltin("cdx", {}).kind, BuiltinKind::None);
}

// ------------------------------------------------------------------ commands

TEST(ConsoleShellClassify, BuiltinsAreKnownWithoutTheDisk)
{
    EXPECT_EQ(ConsoleShell::classifyCommand("cd", "/"), CommandKind::ConsoleBuiltin);
    EXPECT_EQ(ConsoleShell::classifyCommand("export", "/"), CommandKind::ConsoleBuiltin);
}

#ifndef Q_OS_WIN
TEST(ConsoleShellClassify, ProgramsFromPathAndPaths)
{
    EXPECT_EQ(ConsoleShell::classifyCommand("sh", "/"), CommandKind::Executable);
    EXPECT_EQ(ConsoleShell::classifyCommand("/bin/sh", "/"), CommandKind::Executable);
    EXPECT_EQ(ConsoleShell::classifyCommand("./sh", "/bin"), CommandKind::Executable);
    EXPECT_EQ(ConsoleShell::classifyCommand("../bin/sh", "/etc"), CommandKind::Executable);
}

TEST(ConsoleShellClassify, UnknownThings)
{
    EXPECT_EQ(ConsoleShell::classifyCommand("", "/"), CommandKind::Unknown);
    EXPECT_EQ(ConsoleShell::classifyCommand("definitely-not-a-program-xyz", "/"), CommandKind::Unknown);
    EXPECT_EQ(ConsoleShell::classifyCommand("./definitely-not-a-program-xyz", "/bin"), CommandKind::Unknown);
    EXPECT_EQ(ConsoleShell::classifyCommand("/etc", "/"), CommandKind::Unknown); // a directory is not a program
}
#endif

// ------------------------------------------------------------------ pasting

TEST(ConsoleShellPaste, OneLineIsUntouched)
{
    EXPECT_EQ(ConsoleShell::joinPastedLines("ls -l"), "ls -l");
    EXPECT_EQ(ConsoleShell::joinPastedLines("ls -l\n"), "ls -l");
}

TEST(ConsoleShellPaste, LinesBecomeASequence)
{
    EXPECT_EQ(ConsoleShell::joinPastedLines("cd build\n\n  cmake ..\r\nmake\n"), "cd build; cmake ..; make");
}

#ifndef Q_OS_WIN
TEST(ConsoleShellPaste, BackslashContinuesTheCommand)
{
    EXPECT_EQ(ConsoleShell::joinPastedLines("g++ \\\n  -O2 \\\n  main.cpp\nls"), "g++ -O2 main.cpp; ls");
}
#endif
