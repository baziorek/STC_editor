#include <vector>
#include <string>
#include <stdexcept>
#include <QString>
#include <QStringList>
#include <nuspell/dictionary.hxx>
#include "SpellChecker.h"

#ifndef DICTIONARY_PATH
#define DICTIONARY_PATH ""
#endif


SpellChecker::SpellChecker(const QString& aff_path)
{
    const QString resolvedPath = aff_path.isEmpty() ? QStringLiteral(DICTIONARY_PATH) : aff_path;

    try
    {
        dict.load_aff_dic(resolvedPath.toStdString());
    }
    catch (const nuspell::Dictionary_Loading_Error& e)
    {
        throw std::runtime_error(e.what());
    }
}

bool SpellChecker::isCorrect(const QString& word) const
{
    return dict.spell(word.toStdString());
}

int SpellChecker::suggestionsCount(const QString& word) const
{
    std::vector<std::string> suggestions;
    dict.suggest(word.toStdString(), suggestions);
    return static_cast<int>(suggestions.size());
}

QStringList SpellChecker::getSuggestions(const QString& word) const
{
    std::vector<std::string> suggestions;
    dict.suggest(word.toStdString(), suggestions);

    QStringList qSuggestions;
    for (const auto& s : suggestions)
    {
        qSuggestions << QString::fromStdString(s);
    }

    return qSuggestions;
}
