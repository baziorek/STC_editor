#pragma once

#include <optional>
#include <QString>
#include <QTextCursor>

/// What the compiler window (CppCompilerDialog) can do with a block of code
enum class RunnableLanguage
{
    Cpp,    ///< compiled with g++
    Python, ///< the syntax checked with python3 -m py_compile, run with python3
    Bash,   ///< the syntax checked with bash -n, run with bash
};

struct CodeBlock
{
    QTextCursor cursor;
    QString tag;        // "cpp", "code", "py"
    QString language;   // "c++", "python", "" if no `src=`

    /// `[py]...[/py]` or `[code src="python"]...[/code]`
    bool isPython() const
    {
        return tag == QLatin1String("py") || language == QLatin1String("py") || language.contains(QLatin1String("python"));
    }

    /// `[cpp]...[/cpp]` or `[code src="c++"]...[/code]`
    bool isCpp() const
    {
        return tag == QLatin1String("cpp") || language.contains(QLatin1String("cpp")) || language.contains(QLatin1String("c++"));
    }

    /// `[code src="bash"]...[/code]` and a `[code]` without `src=` (in articles that is usually a console session)
    bool isBash() const
    {
        return language == QLatin1String("bash") || language == QLatin1String("sh") || language == QLatin1String("shell")
               || (tag == QLatin1String("code") && language.isEmpty());
    }

    /// The program which checks / runs the block: g++, python3 or bash
    std::optional<RunnableLanguage> runnableLanguage() const
    {
        if (isCpp())
            return RunnableLanguage::Cpp;
        if (isPython())
            return RunnableLanguage::Python;
        if (isBash())
            return RunnableLanguage::Bash;
        return std::nullopt;
    }

    /// Whether the compiler window (g++, python3 or bash) can handle this block
    bool canBeCompiled() const { return runnableLanguage().has_value(); }

    bool operator==(const CodeBlock& codeBlock)
    {
        return cursor.selectionStart() == codeBlock.cursor.selectionStart()
               && cursor.selectionEnd() == codeBlock.cursor.selectionEnd()
               && tag == codeBlock.tag
               && language == codeBlock.language;
    }
};

