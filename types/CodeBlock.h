#pragma once

#include <QString>
#include <QTextCursor>

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

    /// `[cpp]...[/cpp]`, `[code src="c++"]...[/code]` and a `[code]` without `src=` (in articles it is C++ by default)
    bool isCpp() const
    {
        return tag == QLatin1String("cpp") || language.contains(QLatin1String("cpp")) || language.contains(QLatin1String("c++"))
               || (tag == QLatin1String("code") && language.isEmpty());
    }

    /// Whether the compiler window (g++ or python3) can handle this block
    bool canBeCompiled() const { return isPython() || isCpp(); }

    bool operator==(const CodeBlock& codeBlock)
    {
        return cursor.selectionStart() == codeBlock.cursor.selectionStart()
               && cursor.selectionEnd() == codeBlock.cursor.selectionEnd()
               && tag == codeBlock.tag
               && language == codeBlock.language;
    }
};

