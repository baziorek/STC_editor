#include <QRegularExpression>
#include <QFileInfo>
#include "documentstatistics.h"
#include "codeeditor.h"


DocumentStatisticsResult DocumentStatistics::analyze(CodeEditor* editor)
{
    DocumentStatisticsResult result;
    if (!editor)
        return result;

    const QString content = editor->toPlainText();
    const QString filePath = editor->getFileName(); // zakładam, że masz taką metodę

    QFileInfo fi(filePath);
    result.fileName = fi.fileName();
    result.created  = fi.birthTime();
    result.modified = fi.lastModified();

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
    return QString(
               "File: %1\n"
               "Created: %2\n"
               "Modified: %3\n\n"
               "Lines: %4\n"
               "Characters: %5\n"
               "Words: %6\n\n"
               "[h1] sections: %7\n"
               "[h2] sections: %8\n"
               "[h3] sections: %9\n"
               "C++ code sections: %10\n"
               "Links: %11\n"
               "DIV blocks: %12\n"
               "Images: %13")
        .arg(fileName)
        .arg(created.toString(Qt::ISODate))
        .arg(modified.toString(Qt::ISODate))
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
