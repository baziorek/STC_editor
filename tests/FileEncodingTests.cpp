// FileEncodingHandler: the encoding a file was read in (kept for reading) is separate from the one it is saved in
// (chosen by the user, or the same as the file had). Only QtCore.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QStringEncoder>
#include <QTemporaryDir>

#include "utils/FileEncodingHandler.h"

namespace
{
// Long enough for the detection to be sure about the encoding
const QString polishText = QStringLiteral(
    "Zażółć gęślą jaźń. Wkrótce przyjdą źli ludzie, którzy śpiewają pieśni o łódce na Wiśle. ").repeated(12);

QByteArray encodeAs(const QString& encodingName, const QString& text)
{
    QStringEncoder encoder{QByteArray(encodingName.toUtf8())};
    return encoder.encode(text);
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(bytes);
    return true;
}

class FileEncodingTest : public ::testing::Test
{
protected:
    QTemporaryDir dir;
    QString cp1250Path() const { return dir.filePath("cp1250.txt"); }
    QString utf8Path() const { return dir.filePath("utf8.txt"); }
};
} // namespace

TEST_F(FileEncodingTest, LoadedFileIsSavedInTheEncodingItWasReadIn)
{
    ASSERT_TRUE(writeBytes(cp1250Path(), encodeAs("windows-1250", polishText)));

    FileEncodingHandler handler;
    const QString content = handler.loadFile(cp1250Path());

    EXPECT_EQ(content, polishText);
    EXPECT_EQ(handler.lastDetectedEncoding().compare("windows-1250", Qt::CaseInsensitive), 0);
    EXPECT_EQ(handler.targetEncoding().compare("windows-1250", Qt::CaseInsensitive), 0);
}

TEST_F(FileEncodingTest, ChangingTheTargetDoesNotChangeTheEncodingOnDisk)
{
    ASSERT_TRUE(writeBytes(cp1250Path(), encodeAs("windows-1250", polishText)));

    FileEncodingHandler handler;
    handler.loadFile(cp1250Path());
    handler.setTargetEncoding("UTF-8");

    EXPECT_EQ(handler.targetEncoding(), "UTF-8");
    EXPECT_EQ(handler.lastDetectedEncoding().compare("windows-1250", Qt::CaseInsensitive), 0);
}

TEST_F(FileEncodingTest, EncodeUsesTheTargetEncoding)
{
    FileEncodingHandler handler;
    handler.setTargetEncoding("windows-1250");

    bool lossless = false;
    const QByteArray bytes = handler.encode(QStringLiteral("ąćę"), &lossless);

    EXPECT_TRUE(lossless);
    EXPECT_EQ(bytes, QByteArray::fromHex("b9e6ea")); // cp1250: ą=B9, ć=E6, ę=EA
}

TEST_F(FileEncodingTest, CharactersOutsideTheEncodingAreReported)
{
    FileEncodingHandler handler;
    handler.setTargetEncoding("ISO-8859-2");

    bool lossless = true;
    handler.encode(QStringLiteral("ąć€"), &lossless); // the euro sign is not in ISO-8859-2

    EXPECT_FALSE(lossless);
}

TEST_F(FileEncodingTest, ReadingAnotherFileRemembersNothing)
{
    ASSERT_TRUE(writeBytes(cp1250Path(), encodeAs("windows-1250", polishText)));
    ASSERT_TRUE(writeBytes(utf8Path(), QStringLiteral("tekst w UTF-8").toUtf8()));

    FileEncodingHandler handler;
    handler.loadFile(cp1250Path());
    handler.setTargetEncoding("UTF-8");

    QString otherEncoding;
    FileEncodingHandler::readFile(utf8Path(), &otherEncoding);

    EXPECT_EQ(handler.targetEncoding(), "UTF-8");
    EXPECT_EQ(handler.lastDetectedEncoding().compare("windows-1250", Qt::CaseInsensitive), 0);
    EXPECT_FALSE(otherEncoding.isEmpty());
}

TEST_F(FileEncodingTest, SavedFileHasTheTargetEncodingOnDisk)
{
    FileEncodingHandler handler;
    handler.setTargetEncoding("windows-1250");

    ASSERT_TRUE(handler.saveFile(cp1250Path(), polishText));
    EXPECT_EQ(handler.lastDetectedEncoding(), "windows-1250");

    QFile file(cp1250Path());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), encodeAs("windows-1250", polishText));
}

TEST_F(FileEncodingTest, PlainAsciiFileIsSavedAsUtf8SoThatNationalLettersCanBeAdded)
{
    ASSERT_TRUE(writeBytes(utf8Path(), QByteArray("int main() { return 0; }\n")));

    FileEncodingHandler handler;
    handler.loadFile(utf8Path());

    EXPECT_EQ(handler.targetEncoding(), "UTF-8");
    bool lossless = false;
    handler.encode(QStringLiteral("ąę"), &lossless);
    EXPECT_TRUE(lossless);
}

TEST_F(FileEncodingTest, DecodingAsAnotherEncodingReportsBytesWhichDoNotFit)
{
    ASSERT_TRUE(writeBytes(cp1250Path(), encodeAs("windows-1250", polishText)));

    bool validAsCp1250 = false;
    const QString asCp1250 = FileEncodingHandler::decodeFile(cp1250Path(), "windows-1250", &validAsCp1250);
    EXPECT_TRUE(validAsCp1250);
    EXPECT_EQ(asCp1250, polishText);

    // the same bytes read as UTF-8 are not valid UTF-8 text: the user is warned, and it is what the wrong guess looks like
    bool validAsUtf8 = true;
    FileEncodingHandler::decodeFile(cp1250Path(), "UTF-8", &validAsUtf8);
    EXPECT_FALSE(validAsUtf8);
}

TEST_F(FileEncodingTest, RememberedEncodingIsTheOneOnDiskAndTheOneSavedIn)
{
    FileEncodingHandler handler;
    handler.rememberFileEncoding("ISO-8859-2");

    EXPECT_EQ(handler.lastDetectedEncoding(), "ISO-8859-2");
    EXPECT_EQ(handler.targetEncoding(), "ISO-8859-2");
}
