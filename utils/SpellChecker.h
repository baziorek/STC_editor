#pragma once

#include <nuspell/dictionary.hxx>
#include <QString>
#include <QStringList>


class SpellChecker
{
public:
    /// With a path of the `.aff` file the dictionary has to load (std::runtime_error otherwise). Without it the dictionary
    /// is looked for where the program can be (see SpellChecker.cpp); when there is none the program works without the
    /// spell checking (every word is correct, no suggestions), which is better than not starting.
    explicit SpellChecker(const QString& aff_path={});
    /// False when no dictionary was found: nothing is underlined then
    bool isLoaded() const { return loaded; }
    bool isCorrect(const QString& word) const;
    int suggestionsCount(const QString& word) const;
    QStringList getSuggestions(const QString& word) const;

private:
    nuspell::Dictionary dict;
    bool loaded = false;
};
