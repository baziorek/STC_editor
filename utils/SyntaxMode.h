#pragma once

#include <optional>
#include <QString>
#include <QStringList>


/// How the editor treats its whole content: chosen from the "Syntax" menu
/// or detected from the extension of the opened file.
enum class SyntaxMode
{
    Stc,       ///< cpp0x.pl markup; `[cpp]`, `[py]` blocks get their own highlighting inside of it
    PlainText, ///< no markup at all: spell checking plus formatted links and e-mail addresses
    Cpp,       ///< the whole file is C++ code
    Python,    ///< the whole file is Python code
    Xml,       ///< the whole file is XML
    Json,      ///< the whole file is JSON
};

namespace syntaxmode
{
/// Returns the mode for a well known file extension (case insensitive),
/// `std::nullopt` when the extension is unknown - it is up to the caller to choose the fallback.
///
/// `.txt` is deliberately unknown: it is what the STC articles are saved as, so it can't be told from a plain note.
/// Such a file is switched to plain text from the "Syntax" menu (and the choice is remembered, see SyntaxModeMemory).
std::optional<SyntaxMode> detectFromFileName(const QString& fileName);

/// Stable, language independent name of the mode - what is stored in the settings.
QString toString(SyntaxMode mode);
std::optional<SyntaxMode> fromString(const QString& name);

/// True for the modes in which the entire file is a source code: C++, Python, XML and JSON.
constexpr bool isSourceCodeMode(SyntaxMode mode)
{
    return mode == SyntaxMode::Cpp || mode == SyntaxMode::Python || mode == SyntaxMode::Xml || mode == SyntaxMode::Json;
}

/// True when `[b]`, `[cpp]`... are the STC tags (only `Stc`); in every other mode they are just characters.
constexpr bool usesStcMarkup(SyntaxMode mode)
{
    return mode == SyntaxMode::Stc;
}

/// Wildcards (`*.py`, `*.json`, ...) of the files which are opened in a mode other than STC by their extension.
QStringList knownSourceFileWildcards();
} // namespace syntaxmode
