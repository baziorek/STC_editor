#pragma once

#include <optional>
#include <QString>
#include "utils/SyntaxMode.h"

/// Remembers (in QSettings) the syntax mode which the user chose for a particular file, so it is not lost
/// after the editor is closed. It matters most for `.txt`: the articles in STC and the plain notes share the extension.
namespace syntaxmode::memory
{
/// The mode chosen earlier for this file, if any.
std::optional<SyntaxMode> recall(const QString& fileName);

/// Stores the choice; the oldest entries are dropped when there are too many of them.
void remember(const QString& fileName, SyntaxMode mode);

/// Removes the entry - the mode is again decided by the file extension.
void forget(const QString& fileName);
} // namespace syntaxmode::memory
