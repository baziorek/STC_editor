#include "SyntaxMode.h"

#include <QFileInfo>
#include <QHash>

namespace syntaxmode
{
std::optional<SyntaxMode> detectFromFileName(const QString& fileName)
{
    // NOTE: `.ts` is intentionally absent - it is both Qt's translation XML and TypeScript.
    static const QHash<QString, SyntaxMode> byExtension = {
        { "stc", SyntaxMode::Stc },

        { "cpp", SyntaxMode::Cpp }, { "cc", SyntaxMode::Cpp },  { "cxx", SyntaxMode::Cpp },
        { "c", SyntaxMode::Cpp },   { "h", SyntaxMode::Cpp },   { "hpp", SyntaxMode::Cpp },
        { "hh", SyntaxMode::Cpp },  { "hxx", SyntaxMode::Cpp }, { "ipp", SyntaxMode::Cpp },
        { "tpp", SyntaxMode::Cpp }, { "inl", SyntaxMode::Cpp },

        { "py", SyntaxMode::Python }, { "pyw", SyntaxMode::Python }, { "pyi", SyntaxMode::Python },

        { "xml", SyntaxMode::Xml },  { "xsd", SyntaxMode::Xml },   { "xsl", SyntaxMode::Xml },
        { "xslt", SyntaxMode::Xml }, { "svg", SyntaxMode::Xml },   { "ui", SyntaxMode::Xml },
        { "qrc", SyntaxMode::Xml },  { "xhtml", SyntaxMode::Xml }, { "plist", SyntaxMode::Xml },

        { "json", SyntaxMode::Json }, { "geojson", SyntaxMode::Json },
    };

    const auto it = byExtension.constFind(QFileInfo(fileName).suffix().toLower());
    if (it == byExtension.cend())
        return std::nullopt;
    return it.value();
}
} // namespace syntaxmode
