#pragma once

#include <nuspell/dictionary.hxx>
#include <QString>
#include <QStringList>


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
