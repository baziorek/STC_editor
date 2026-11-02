#include <nuspell/dictionary.hxx>
#include <vector>
#include <string>
#include <stdexcept>
#include <QString>
#include <QStringList>
#include <QList>
#include <QStringLiteral>
#include <filesystem>

#include "SpellChecker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#ifndef DICTIONARY_PATH
#define DICTIONARY_PATH ""
#endif


namespace
{
/// The default dictionary: the one of the source tree the program was built from (development), next to the program
/// (a zip, an AppImage: `dictionaries/pl/` is copied beside the executable) and where `cmake --install` puts it
/// (`<prefix>/share/STC_editor/dictionaries/pl/`, which is what the .deb has).
QString findDefaultDictionary()
{
    const QString relative = QStringLiteral("dictionaries/pl/pl_PL.aff");
    const QString appDir = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();

    QStringList candidates{QStringLiteral(DICTIONARY_PATH)};
    if (!appDir.isEmpty())
    {
        candidates << appDir + QLatin1Char('/') + relative
                   << appDir + QStringLiteral("/../share/STC_editor/") + relative;
    }

    for (const QString& candidate : std::as_const(candidates))
    {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return {};
}
} // namespace

SpellChecker::SpellChecker(const QString& aff_path)
{
    const QString resolvedPath = aff_path.isEmpty() ? findDefaultDictionary() : aff_path;

    if (resolvedPath.isEmpty())
    {
        qWarning() << "No Polish dictionary found (dictionaries/pl/pl_PL.aff): the spell checking is off";
        return;
    }

    try
    {
        dict.load_aff_dic(resolvedPath.toStdString());
        loaded = true;
    }
    catch (const nuspell::Dictionary_Loading_Error& e)
    {
        throw std::runtime_error(e.what());
    }
}

bool SpellChecker::isCorrect(const QString& word) const
{
    return !loaded || dict.spell(word.toStdString());
}

int SpellChecker::suggestionsCount(const QString& word) const
{
    if (!loaded)
        return 0;
    std::vector<std::string> suggestions;
    dict.suggest(word.toStdString(), suggestions);
    return static_cast<int>(suggestions.size());
}

QStringList SpellChecker::getSuggestions(const QString& word) const
{
    QStringList qSuggestions;
    if (!loaded)
        return qSuggestions;

    std::vector<std::string> suggestions;
    dict.suggest(word.toStdString(), suggestions);

    for (const auto& s : suggestions)
    {
        qSuggestions << QString::fromStdString(s);
    }

    return qSuggestions;
}
