#include <QFile>
#include <QByteArray>
#include <QStringDecoder>
#include <QStringEncoder>
#include <stdexcept>
#include <QAnyStringView>
#include <QChar>
#include <QIODevice>
#include <QLatin1String>
#include <QList>
#include <QStringList>
#include <QStringLiteral>
#include <Qt>
#include <QtGlobal>
#include <string>

// https://gitlab.freedesktop.org/uchardet/uchardet
#if __has_include(<uchardet.h>) // when installing with FetchContent_Declare from CMakeLists.txt
    #include <uchardet.h>
#elif __has_include(<uchardet/uchardet.h>) // when installing with package manager (Linux)
    #include <uchardet/uchardet.h>
#else
    #error "uchardet not found in directories neither <uchardet.h> nor <uchardet/uchardet.h>"
#endif

#include "FileEncodingHandler.h"

namespace
{
/// The encoding uchardet finds in the data; UTF-8 when it finds none
QString detectEncoding(const QByteArray& data)
{
    uchardet_t detector = uchardet_new();

    if (uchardet_handle_data(detector, data.constData(), data.size()) != 0)
    {
        uchardet_delete(detector);
        throw std::runtime_error("uchardet failed to handle data");
    }

    uchardet_data_end(detector);
    // uchardet_get_charset() is deprecated in favor of a candidate-based API
    // (uchardet_get_n_candidates()/uchardet_get_encoding()), but that API is
    // only available in uchardet >= 0.1.0 - the version most distros package
    // (e.g. Ubuntu/Kubuntu ship 0.0.8) doesn't have it at all. Silencing the
    // warning is the only option that works against every uchardet we might
    // link against; switching APIs isn't a safe option here.
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wdeprecated-declarations")
QT_WARNING_DISABLE_CLANG("-Wdeprecated-declarations")
    const char* charset = uchardet_get_charset(detector);
QT_WARNING_POP

    const QString name = charset ? QString::fromUtf8(charset).trimmed() : QString();
    uchardet_delete(detector);

    return name.isEmpty() ? QStringLiteral("UTF-8") : name;
}
} // namespace

struct FileEncodingHandler::Impl
{
    QString encodingName;    ///< the encoding of the file on disk
    QString targetEncoding;  ///< the encoding the file is saved in
};

FileEncodingHandler::FileEncodingHandler()
    : impl(std::make_unique<Impl>())
{}

FileEncodingHandler::~FileEncodingHandler() = default;

QString FileEncodingHandler::lastDetectedEncoding() const
{
    return impl->encodingName;
}

QString FileEncodingHandler::targetEncoding() const
{
    return impl->targetEncoding;
}

void FileEncodingHandler::setTargetEncoding(const QString& encodingName)
{
    impl->targetEncoding = encodingName;
}

void FileEncodingHandler::noteWrittenInTargetEncoding()
{
    impl->encodingName = impl->targetEncoding;
}

QString FileEncodingHandler::readFile(const QString& filePath, QString* detectedEncoding)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        throw std::runtime_error(("Cannot open file: " + filePath).toStdString());
    }

    QByteArray data = file.readAll();
    file.close();

    QString encodingName = detectEncoding(data);

    // Plain ASCII is a part of UTF-8: a file which has no national characters yet is saved as UTF-8, so that the first
    // Polish letter typed into it does not make saving ask about characters which cannot be written
    if (encodingName.compare(QLatin1String("ASCII"), Qt::CaseInsensitive) == 0
        || encodingName.compare(QLatin1String("US-ASCII"), Qt::CaseInsensitive) == 0)
    {
        encodingName = QStringLiteral("UTF-8");
    }

    // If the encoding is unknown to Qt, fallback to UTF-8 (and say so, so that the menu does not show a name that is not used)
    QStringDecoder decoder(encodingName.toUtf8());
    if (!decoder.isValid())
    {
        decoder = QStringDecoder(QStringDecoder::Utf8);
        encodingName = QStringLiteral("UTF-8");
    }

    if (detectedEncoding)
        *detectedEncoding = encodingName;

    return decoder.decode(data);
}

QString FileEncodingHandler::decodeFile(const QString& filePath, const QString& encodingName, bool* valid)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        throw std::runtime_error(("Cannot open file: " + filePath).toStdString());
    }

    const QByteArray data = file.readAll();
    file.close();

    QStringDecoder decoder(encodingName.toUtf8());
    if (!decoder.isValid())
        decoder = QStringDecoder(QStringDecoder::Utf8);

    QString text = decoder.decode(data);
    if (valid)
        *valid = !decoder.hasError();
    return text;
}

void FileEncodingHandler::rememberFileEncoding(const QString& encodingName)
{
    impl->encodingName = encodingName;
    impl->targetEncoding = encodingName;
}

QString FileEncodingHandler::loadFile(const QString& filePath)
{
    QString encodingName;
    QString content = readFile(filePath, &encodingName);

    impl->encodingName = encodingName;
    impl->targetEncoding = encodingName;

    return content;
}

QByteArray FileEncodingHandler::encode(const QString& content, bool* lossless) const
{
    QStringEncoder encoder(impl->targetEncoding.toUtf8());
    if (!encoder.isValid())
        encoder = QStringEncoder(QStringEncoder::Utf8);

    QByteArray bytes = encoder.encode(content);
    if (lossless)
        *lossless = !encoder.hasError();
    return bytes;
}

bool FileEncodingHandler::saveFile(const QString& filePath, const QString& content)
{
    QFile file(filePath);
    if (!file.open(QFile::WriteOnly))
        return false;

    file.write(encode(content));
    file.close();

    noteWrittenInTargetEncoding();
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
    {
QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wdeprecated-declarations")
QT_WARNING_DISABLE_CLANG("-Wdeprecated-declarations")
        detectedCharset = QString::fromUtf8(uchardet_get_charset(detector)).trimmed();
QT_WARNING_POP
    }

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
