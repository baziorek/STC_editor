#include "SyntaxMode.h"

#include <QFileInfo>
#include <QHash>
#include <QMap>

namespace syntaxmode
{
namespace
{
const QMap<QString, SyntaxMode>& extensionTable()
{
    // NOTE: `.ts` is intentionally absent - it is both Qt's translation XML and TypeScript.
    static const QMap<QString, SyntaxMode> byExtension = {
        { "cpp", SyntaxMode::Cpp }, { "cc", SyntaxMode::Cpp },  { "cxx", SyntaxMode::Cpp },
        { "c", SyntaxMode::Cpp },   { "h", SyntaxMode::Cpp },   { "hpp", SyntaxMode::Cpp },
        { "hh", SyntaxMode::Cpp },  { "hxx", SyntaxMode::Cpp }, { "ipp", SyntaxMode::Cpp },
        { "tpp", SyntaxMode::Cpp }, { "inl", SyntaxMode::Cpp },

        { "py", SyntaxMode::Python }, { "pyw", SyntaxMode::Python }, { "pyi", SyntaxMode::Python },

        { "xml", SyntaxMode::Xml },  { "xsd", SyntaxMode::Xml },   { "xsl", SyntaxMode::Xml },
        { "xslt", SyntaxMode::Xml }, { "svg", SyntaxMode::Xml },   { "ui", SyntaxMode::Xml },
        { "qrc", SyntaxMode::Xml },  { "xhtml", SyntaxMode::Xml }, { "plist", SyntaxMode::Xml },

        { "json", SyntaxMode::Json }, { "geojson", SyntaxMode::Json },

        // prose which has no STC tags
        { "md", SyntaxMode::PlainText }, { "markdown", SyntaxMode::PlainText },
        { "rst", SyntaxMode::PlainText }, { "text", SyntaxMode::PlainText },
    };
    return byExtension;
}

struct ModeName
{
    SyntaxMode mode;
    const char* name;
};
constexpr ModeName modeNames[] = {
    { SyntaxMode::Stc, "stc" },       { SyntaxMode::PlainText, "plaintext" }, { SyntaxMode::Cpp, "cpp" },
    { SyntaxMode::Python, "python" }, { SyntaxMode::Xml, "xml" },             { SyntaxMode::Json, "json" },
};
} // namespace

std::optional<SyntaxMode> detectFromFileName(const QString& fileName)
{
    const auto& table = extensionTable();
    const auto it = table.constFind(QFileInfo(fileName).suffix().toLower());
    if (it == table.cend())
        return std::nullopt;
    return it.value();
}

QString toString(SyntaxMode mode)
{
    for (const auto& entry : modeNames)
        if (entry.mode == mode)
            return QString::fromLatin1(entry.name);
    return QStringLiteral("stc");
}

std::optional<SyntaxMode> fromString(const QString& name)
{
    for (const auto& entry : modeNames)
        if (name == QLatin1String(entry.name))
            return entry.mode;
    return std::nullopt;
}

QStringList knownSourceFileWildcards()
{
    QStringList result;
    for (auto it = extensionTable().cbegin(); it != extensionTable().cend(); ++it)
        if (isSourceCodeMode(it.value()))
            result << "*." + it.key();
    return result;
}
} // namespace syntaxmode
