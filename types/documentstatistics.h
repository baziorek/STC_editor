#pragma once

#include <QString>
#include <QDateTime>

class CodeEditor;

struct DocumentStatisticsResult
{
    QString fileName;
    QString filePath;
    QDateTime created;
    QDateTime modified;
    qint64 fileSizeBytes = 0;
    bool isReadable = false;
    bool isWritable = false;
    bool isExecutable = false;

    int lineCount = 0;
    int charCount = 0;
    int wordCount = 0;

    int h1Count = 0;
    int h2Count = 0;
    int h3Count = 0;

    int cppCodeCount = 0;
    int linkCount = 0;
    int divCount = 0;
    int imageCount = 0;

    QString toQString() const;
};

namespace DocumentStatistics
{
    DocumentStatisticsResult analyze(CodeEditor* editor);
};
