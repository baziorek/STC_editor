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

QString makeFileStatsHtml(
    const QString &fileName,
    const QString &filePath,
    const QString &sizeStr,
    qint64 fileSizeBytes,
    const QString &permissions,
    const QDateTime &created,
    const QDateTime &modified,
    int lineCount,
    int charCount,
    int wordCount,
    int h1Count,
    int h2Count,
    int h3Count,
    int cppCodeCount,
    int linkCount,
    int divCount,
    int imageCount)
{
    const QString htmlTemplate = QStringLiteral(R"(
<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>File statistics</title>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial; background:#f6f8fa; color:#222; margin:20px; }
  .card { background:white; border-radius:8px; box-shadow:0 6px 18px rgba(0,0,0,0.08); padding:20px; max-width:900px; margin:0 auto; }
  .header { display:flex; justify-content:space-between; align-items:center; gap:20px; }
  .meta { font-size:14px; color:#555; }
  .title { font-size:20px; font-weight:700; margin:0; }
  .badge { display:inline-block; padding:6px 10px; border-radius:999px; background:#eef6ff; color:#1a73e8; font-weight:600; font-size:13px; }
  .grid { display:grid; grid-template-columns: 1fr 1fr; gap:16px; margin-top:18px; }
  table.stats { width:100%; border-collapse:collapse; }
  table.stats td { padding:8px 6px; border-bottom:1px solid #f0f0f0; vertical-align:top; }
  table.stats td.key { width:50%; color:#444; font-weight:600; }
  .small { font-size:13px; color:#666; }
  .section-title { margin-top:22px; font-size:16px; font-weight:700; color:#333; }
  .footer-note { margin-top:18px; font-size:12px; color:#888; }
  @media (max-width:600px) { .grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
  <div class="card">
    <div class="header">
      <div>
        <p class="title">File: <span class="badge">%1</span></p>
        <p class="meta">Path: <span class="small">%2</span></p>
      </div>
      <div style="text-align:right">
        <p class="meta">Size: <strong>%3</strong> <span class="small">(%4 bytes)</span></p>
        <p class="meta">Permissions: <strong>%5</strong></p>
        <p class="meta">Created: <span class="small">%6</span></p>
        <p class="meta">Modified: <span class="small">%7</span></p>
      </div>
    </div>

    <div class="section-title">Content statistics</div>
    <div class="grid">
      <div>
        <table class="stats">
          <tr><td class="key">Lines</td><td>%8</td></tr>
          <tr><td class="key">Characters</td><td>%9</td></tr>
          <tr><td class="key">Words</td><td>%10</td></tr>
          <tr><td class="key">C++ code sections</td><td>%14</td></tr>
        </table>
      </div>
      <div>
        <table class="stats">
          <tr><td class="key">[h1] sections</td><td>%11</td></tr>
          <tr><td class="key">[h2] sections</td><td>%12</td></tr>
          <tr><td class="key">[h3] sections</td><td>%13</td></tr>
          <tr><td class="key">Links</td><td>%15</td></tr>
          <tr><td class="key">DIV blocks</td><td>%16</td></tr>
          <tr><td class="key">Images</td><td>%17</td></tr>
        </table>
      </div>
    </div>

    <div class="footer-note">
      Generated statistics for the scanned file.
    </div>
  </div>
</body>
</html>
)");

    return htmlTemplate
        .arg(fileName)
        .arg(filePath)
        .arg(sizeStr)
        .arg(QString::number(fileSizeBytes))
        .arg(permissions)
        .arg(created.toString("yyyy-MM-dd hh:mm:ss"))
        .arg(modified.toString("yyyy-MM-dd hh:mm:ss"))
        .arg(QString::number(lineCount))
        .arg(QString::number(charCount))
        .arg(QString::number(wordCount))
        .arg(QString::number(h1Count))
        .arg(QString::number(h2Count))
        .arg(QString::number(h3Count))
        .arg(QString::number(cppCodeCount))
        .arg(QString::number(linkCount))
        .arg(QString::number(divCount))
        .arg(QString::number(imageCount));
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

    return makeFileStatsHtml(
        fileName,
        filePath,
        sizeStr,
        fileSizeBytes,
        permissions,
        created,
        modified,
        lineCount,
        charCount,
        wordCount,
        h1Count,
        h2Count,
        h3Count,
        cppCodeCount,
        linkCount,
        divCount,
        imageCount
        );
}
