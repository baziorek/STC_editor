#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include "utils/ConsoleShell.h"

class QTextDocument;

/// Colors the command line of the console: the command (green when the program exists, blue for a shell built-in,
/// red when it does not exist), options, strings, variables, operators and comments.
/// The tokens come from `ConsoleShell::tokenize()`.
class ConsoleSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit ConsoleSyntaxHighlighter(QTextDocument* document);

    /// Relative paths of programs (`./build/app`) are looked for here.
    void setWorkingDirectory(const QString& directory);

    /// The colors suit a dark or a light background; call it again when the palette changes.
    void setDarkBackground(bool dark);

protected:
    void highlightBlock(const QString& text) override;

private:
    ConsoleShell::CommandKind commandKind(const QString& word);
    QTextCharFormat formatFor(ConsoleShell::TokenKind kind) const;
    void rebuildFormats();

    QString workingDirectory_;
    bool dark_ = false;

    QTextCharFormat commandFound_, commandBuiltin_, commandUnknown_, assignment_, option_, string_, variable_, operator_,
        comment_;

    // The disk is asked once per word and a few seconds: the line is highlighted again after every typed character
    QHash<QString, ConsoleShell::CommandKind> commandCache_;
    QElapsedTimer cacheAge_;
};
