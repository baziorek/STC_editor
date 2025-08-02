#pragma once

#include <QString>
#include <nuspell/dictionary.hxx>


class SpellChecker
{
public:
    explicit SpellChecker(const QString& aff_path={});
    bool isCorrect(const QString& word) const;
    int suggestionsCount(const QString& word) const;
    QStringList getSuggestions(const QString& word) const;

private:
    nuspell::Dictionary dict;
};
