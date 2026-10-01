#include "SyntaxModeMemory.h"

#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace syntaxmode::memory
{
namespace
{
constexpr auto settingsKey = "SyntaxModeByFile";
constexpr int maxEntries = 500;
constexpr QChar separator = u'|';

QString normalized(const QString& fileName)
{
    return QFileInfo(fileName).absoluteFilePath();
}

/// An entry is `<mode name>|<absolute path>`; the newest entries are at the end.
QStringList load()
{
    return QSettings().value(settingsKey).toStringList();
}

void store(const QStringList& entries)
{
    QSettings settings;
    if (entries.isEmpty())
        settings.remove(settingsKey);
    else
        settings.setValue(settingsKey, entries);
}

bool isEntryOf(const QString& entry, const QString& path)
{
    const int at = entry.indexOf(separator);
    return at >= 0 && QStringView(entry).mid(at + 1) == path;
}
} // namespace

std::optional<SyntaxMode> recall(const QString& fileName)
{
    if (fileName.isEmpty())
        return std::nullopt;

    const QString path = normalized(fileName);
    const QStringList entries = load();
    for (auto it = entries.crbegin(); it != entries.crend(); ++it)
        if (isEntryOf(*it, path))
            return fromString(it->left(it->indexOf(separator)));
    return std::nullopt;
}

void remember(const QString& fileName, SyntaxMode mode)
{
    if (fileName.isEmpty())
        return;

    const QString path = normalized(fileName);
    QStringList entries = load();
    entries.removeIf([&path](const QString& entry) { return isEntryOf(entry, path); });
    entries.append(toString(mode) + separator + path);
    while (entries.size() > maxEntries)
        entries.removeFirst();
    store(entries);
}

void forget(const QString& fileName)
{
    if (fileName.isEmpty())
        return;

    const QString path = normalized(fileName);
    QStringList entries = load();
    const auto removed = entries.removeIf([&path](const QString& entry) { return isEntryOf(entry, path); });
    if (removed > 0)
        store(entries);
}
} // namespace syntaxmode::memory
