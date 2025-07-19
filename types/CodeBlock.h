#pragma once

#include <QString>
#include <QTextCursor>

struct CodeBlock
{
    QTextCursor cursor;
    QString tag;        // "cpp", "code", "py"
    QString language;   // "c++", "python", "" if no `src=`

    bool operator==(const CodeBlock& codeBlock)
    {
        return cursor.selectionStart() == codeBlock.cursor.selectionStart()
               && cursor.selectionEnd() == codeBlock.cursor.selectionEnd()
               && tag == codeBlock.tag
               && language == codeBlock.language;
    }
};

