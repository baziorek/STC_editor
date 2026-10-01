#pragma once

#include <optional>
#include <QString>


/// How the editor treats its whole content: chosen from the "Syntax" menu
/// or detected from the extension of the opened file.
enum class SyntaxMode
{
    Stc,    ///< cpp0x.pl markup; `[cpp]`, `[py]` blocks get their own highlighting inside of it
    Cpp,    ///< the whole file is C++ code
    Python, ///< the whole file is Python code
    Xml,    ///< the whole file is XML
    Json,   ///< the whole file is JSON
};

namespace syntaxmode
{
/// Returns the mode for a well known file extension (case insensitive),
/// `std::nullopt` when the extension is unknown - it is up to the caller to choose the fallback.
std::optional<SyntaxMode> detectFromFileName(const QString& fileName);

/// True for the modes in which the entire file is a source code (everything but STC).
constexpr bool isSourceCodeMode(SyntaxMode mode)
{
    return mode != SyntaxMode::Stc;
}
} // namespace syntaxmode
