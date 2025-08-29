#include <QRegularExpression>
#include <QFileInfo>
#include "documentstatistics.h"
#include "CodeEditor.h"


namespace
{
QString sizeInHumenReadable(qint64 fileSizeBytes)
{
    QString sizeStr;
    constexpr qint64 kb = 1024;
    constexpr qint64 mb = kb * 1024;
    constexpr qint64 gb = mb * 1024;

    if (fileSizeBytes >= gb)
    {
        sizeStr = QString("%1 GB").arg(static_cast<double>(fileSizeBytes) / gb, 0, 'f', 2);
    }
    else if (fileSizeBytes >= mb)
    {
        sizeStr = QString("%1 MB").arg(static_cast<double>(fileSizeBytes) / mb, 0, 'f', 2);
    }
    else if (fileSizeBytes >= kb)
    {
        sizeStr = QString("%1 KB").arg(static_cast<double>(fileSizeBytes) / kb, 0, 'f', 2);
    }
    else
    {
        sizeStr = QString("%1 bytes").arg(fileSizeBytes);
    }
    return sizeStr;
}

QString permissionsInHumanReadable(bool isReadable, bool isWritable, bool isExecutable)
{
    QString permissions;
    permissions += isReadable ? "r" : "-";
    permissions += isWritable ? "w" : "-";
    permissions += isExecutable ? "x" : "-";
    return permissions;
}
} // namespace

DocumentStatisticsResult DocumentStatistics::analyze(CodeEditor* editor)
{
    DocumentStatisticsResult result;
    if (!editor)
        return result;

    const QString content = editor->toPlainText();
    const QString filePath = editor->getFileName();

    QFileInfo fi(filePath);
    result.fileName = fi.fileName();
    result.filePath = fi.absoluteFilePath();
    result.created  = fi.birthTime();
    result.modified = fi.lastModified();
    result.fileSizeBytes = fi.size();
    result.isReadable = fi.isReadable();
    result.isWritable = fi.isWritable();
    result.isExecutable = fi.isExecutable();

    result.lineCount = content.count('\n') + 1;
    result.charCount = content.size();
    result.wordCount = content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();

    auto countTag = [&](const QString& tag) {
        QRegularExpression rx(QString(R"(\[%1(?:\s+[^\]]*)?\].*?\[/%1\])").arg(tag),
                              QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
        return content.count(rx);
    };

    result.h1Count = countTag("h1");
    result.h2Count = countTag("h2");
    result.h3Count = countTag("h3");

    result.cppCodeCount = content.count(QRegularExpression(R"(\[(cpp|code\s+src\s*=\s*\"c\+\+\").*?\])",
                                                           QRegularExpression::CaseInsensitiveOption));
    result.linkCount = content.count(QRegularExpression(R"(\[a\s+(?=[^\]]*href\s*=\s*\"[^\"]+\")([^\]]*)\])",
                                                        QRegularExpression::CaseInsensitiveOption));
    result.divCount = countTag("div");
    result.imageCount = content.count(QRegularExpression(R"(\[img\s+src\s*=\s*\"[^\"]+\"\])",
                                                         QRegularExpression::CaseInsensitiveOption));

    return result;
}

QString DocumentStatisticsResult::toQString() const
{
    const auto sizeStr = sizeInHumenReadable(fileSizeBytes);
    const auto permissions = permissionsInHumanReadable(isReadable, isWritable, isExecutable);

    return QString(
               "File: %1\n"
               "Path: %2\n"
               "Size: %3 (%4 bytes)\n"
               "Permissions: %5\n"
               "Created: %6\n"
               "Modified: %7\n\n"
               "Lines: %8\n"
               "Characters: %9\n"
               "Words: %10\n\n"
               "[h1] sections: %11\n"
               "[h2] sections: %12\n"
               "[h3] sections: %13\n"
               "C++ code sections: %14\n"
               "Links: %15\n"
               "DIV blocks: %16\n"
               "Images: %17")
        .arg(fileName)
        .arg(filePath)
        .arg(sizeStr)
        .arg(fileSizeBytes)
        .arg(permissions)
        .arg(created.toString("yyyy-MM-dd hh:mm:ss"))
        .arg(modified.toString("yyyy-MM-dd hh:mm:ss"))
        .arg(lineCount)
        .arg(charCount)
        .arg(wordCount)
        .arg(h1Count)
        .arg(h2Count)
        .arg(h3Count)
        .arg(cppCodeCount)
        .arg(linkCount)
        .arg(divCount)
        .arg(imageCount);
}
