#pragma once

#include <memory>
#include <QString>

class FileEncodingHandler
{
public:
    FileEncodingHandler();
    ~FileEncodingHandler();

    // Load file with encoding detection: returns Unicode QString
    QString loadFile(const QString& filePath);

    // Save file preserving original encoding or fallback
    bool saveFile(const QString& filePath, const QString& content);

    // Returns last used encoding name (eg. "UTF-8", "windows-1250")
    QString lastDetectedEncoding() const;

    bool overwriteLine(const QString& filePath, int lineNumber, const QString& newLineContent);

    bool isProbablyTextFile(const QString &filePath, int maxBytesToCheck = 2048);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
