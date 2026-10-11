#pragma once

#include <memory>
#include <QString>

class FileEncodingHandler
{
public:
    FileEncodingHandler();
    ~FileEncodingHandler();

    /// Reads the file being edited: detects its encoding and remembers it both as the encoding the file is on disk in
    /// and as the encoding it is saved in (until the user chooses another one with setTargetEncoding())
    QString loadFile(const QString& filePath);

    /// Reads any other file (a file pasted into the text, a file compared with the document). Nothing is remembered.
    /// `detectedEncoding` (if given) receives the encoding the file was read in.
    static QString readFile(const QString& filePath, QString* detectedEncoding = nullptr);

    /// The text as bytes in the target encoding. `lossless` (if given) becomes false when some characters cannot be
    /// written in that encoding; they are written as '?'.
    QByteArray encode(const QString& content, bool* lossless = nullptr) const;

    /// Decodes a file as the given encoding (not a detected one). `valid` (if given) becomes false when the file has
    /// bytes which are not valid in that encoding; they are then shown as replacement characters.
    static QString decodeFile(const QString& filePath, const QString& encodingName, bool* valid = nullptr);

    /// The file being edited is in this encoding: on disk, and as the one it is saved in (after the user has chosen
    /// it when the detection was wrong)
    void rememberFileEncoding(const QString& encodingName);

    /// Encoding the file has on disk (detected when it was read, or the target encoding after it was written)
    QString lastDetectedEncoding() const;

    /// Encoding the file is saved in ("UTF-8", "windows-1250", ...)
    QString targetEncoding() const;
    void setTargetEncoding(const QString& encodingName);

    /// Called after the file was written in the target encoding: that is the encoding it has on disk now
    void noteWrittenInTargetEncoding();

    // Save file in the target encoding
    bool saveFile(const QString& filePath, const QString& content);

    bool overwriteLine(const QString& filePath, int lineNumber, const QString& newLineContent);

    bool isProbablyTextFile(const QString &filePath, int maxBytesToCheck = 2048);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
