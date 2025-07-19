#include <QFile>
#include <QByteArray>
#include <QStringDecoder>
#include <QStringEncoder>
#include <stdexcept>
#include <uchardet/uchardet.h>  // https://github.com/BYVoid/uchardet
#include "FileEncodingHandler.h"

struct FileEncodingHandler::Impl
{
    QString encodingName;
};

FileEncodingHandler::FileEncodingHandler()
    : impl(std::make_unique<Impl>())
{}

FileEncodingHandler::~FileEncodingHandler() = default;

QString FileEncodingHandler::lastDetectedEncoding() const
{
    return impl->encodingName;
}

QString FileEncodingHandler::loadFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        throw std::runtime_error(("Cannot open file: " + filePath).toStdString());
    }

    QByteArray data = file.readAll();
    file.close();

    // uchardet setup
    uchardet_t detector = uchardet_new();

    if (uchardet_handle_data(detector, data.constData(), data.size()) != 0)
    {
        uchardet_delete(detector);
        throw std::runtime_error("uchardet failed to handle data");
    }

    uchardet_data_end(detector);
    const char* charset = uchardet_get_charset(detector);

    // If detection failed, fallback to UTF-8
    if (!charset || strlen(charset) == 0)
    {
        impl->encodingName = "UTF-8";
    }
    else
    {
        impl->encodingName = QString::fromUtf8(charset);
    }

    uchardet_delete(detector);

    // Decode using detected encoding
    QStringDecoder decoder(impl->encodingName.toUtf8());
    if (!decoder.isValid())
        decoder = QStringDecoder(QStringDecoder::Utf8);

    return decoder.decode(data);
}

bool FileEncodingHandler::saveFile(const QString& filePath, const QString& content)
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
        return false;

    QStringEncoder encoder(impl->encodingName.toUtf8());
    QByteArray encoded = encoder.encode(content);

    file.write(encoded);
    file.close();
    return true;
}

bool FileEncodingHandler::overwriteLine(const QString& filePath, int lineNumber, const QString& newLineContent)
{
    if (lineNumber < 0)
        return false;

    // Load full content using proper encoding
    QString content;
    try
    {
        content = loadFile(filePath);
    }
    catch (...)
    {
        return false;
    }

    QStringList lines = content.split(QChar::LineFeed, Qt::KeepEmptyParts);
    if (lineNumber >= lines.size())
        return false;

    // Replace specific line
    lines[lineNumber] = newLineContent;

    QString updatedContent = lines.join('\n');
    return saveFile(filePath, updatedContent);
}

bool FileEncodingHandler::isProbablyTextFile(const QString& filePath, int maxBytesToCheck)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray buffer = file.read(maxBytesToCheck);
    file.close();

    if (buffer.isEmpty())
        return false;

    uchardet_t detector = uchardet_new();
    int result = uchardet_handle_data(detector, buffer.constData(), buffer.size());
    uchardet_data_end(detector);

    QString detectedCharset;
    if (result == 0)
        detectedCharset = QString::fromUtf8(uchardet_get_charset(detector)).trimmed();

    uchardet_delete(detector);

    if (detectedCharset.isEmpty())
        return false;

    // Accept only common text encodings — to avoid false positives from binary files
    static const QStringList commonTextEncodings = {
        "UTF-8", "ASCII", "ISO-8859", "windows-125", "UTF-16", "UTF-32"
    };

    for (const QString& pattern : commonTextEncodings)
    {
        if (detectedCharset.startsWith(pattern, Qt::CaseInsensitive))
            return true;
    }

    // Unknown or rare encoding → treat as non-text
    return false;
}
