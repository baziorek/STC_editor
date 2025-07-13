#pragma once

#include <QString>
#include <QTextCursor>

struct CodeBlock
{
    QTextCursor cursor;
    QString tag;        // "cpp", "code", "py"
    QString language;   // "c++", "python", "" if no `src=`
};
