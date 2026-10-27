#include "ConsoleInputEdit.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMimeData>
#include <QPalette>
#include <QTextDocument>
#include <Qt>

#include "utils/ConsoleShell.h"
#include "widgets/ConsoleSyntaxHighlighter.h"

namespace
{
constexpr qsizetype kMaxHistory = 500;
} // namespace

ConsoleInputEdit::ConsoleInputEdit(QWidget* parent) : QPlainTextEdit(parent)
{
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(false);
    setUndoRedoEnabled(true);

    highlighter_ = new ConsoleSyntaxHighlighter(document());
    updateColorsForPalette();
    updateHeight();
}

void ConsoleInputEdit::setHistory(const QStringList& commands)
{
    history_ = commands;
    while (history_.size() > kMaxHistory)
        history_.removeFirst();
    resetHistoryPosition();
}

bool ConsoleInputEdit::addToHistory(const QString& command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return false;

    bool changed = false;
    if (history_.isEmpty() || history_.last() != trimmed)
    {
        history_.append(trimmed);
        changed = true;
    }
    while (history_.size() > kMaxHistory)
        history_.removeFirst();
    resetHistoryPosition();
    return changed;
}

void ConsoleInputEdit::setWorkingDirectory(const QString& directory)
{
    highlighter_->setWorkingDirectory(directory);
}

void ConsoleInputEdit::resetHistoryPosition()
{
    historyIndex_ = history_.size();
    draft_.clear();
}

void ConsoleInputEdit::navigateHistory(int direction)
{
    if (history_.isEmpty())
        return;

    const qsizetype target = historyIndex_ + direction;
    if (target < 0 || target > history_.size())
        return;

    if (historyIndex_ == history_.size())
        draft_ = toPlainText(); // leaving the new command, it is not lost
    historyIndex_ = target;

    setPlainText(target == history_.size() ? draft_ : history_.at(target));
    moveCursor(QTextCursor::End);
}

void ConsoleInputEdit::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool noModifiers = modifiers == Qt::NoModifier;
    const bool control = modifiers == Qt::ControlModifier;

    switch (event->key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (noModifiers || modifiers == Qt::ShiftModifier)
        {
            const QString command = toPlainText();
            clear();
            resetHistoryPosition();
            emit commandSubmitted(command);
            return;
        }
        break;

    case Qt::Key_Up:
        if (noModifiers)
        {
            navigateHistory(-1);
            return;
        }
        if (control)
        {
            emit previousCommandRequested();
            return;
        }
        break;

    case Qt::Key_Down:
        if (noModifiers)
        {
            navigateHistory(+1);
            return;
        }
        if (control)
        {
            emit nextCommandRequested();
            return;
        }
        break;

    case Qt::Key_C:
        if (control && !textCursor().hasSelection())
        {
            emit interruptRequested();
            return;
        }
        break;

    case Qt::Key_L:
        if (control)
        {
            emit clearOutputRequested();
            return;
        }
        break;

    case Qt::Key_D:
        if (control && document()->isEmpty())
        {
            emit endOfInputRequested();
            return;
        }
        break;

    case Qt::Key_Escape:
        if (noModifiers)
        {
            emit leaveRequested();
            return;
        }
        break;

    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        event->accept(); // no tab characters in a command (the completion of names may come here one day)
        return;

    default:
        break;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void ConsoleInputEdit::insertFromMimeData(const QMimeData* source)
{
    if (!source->hasText())
    {
        QPlainTextEdit::insertFromMimeData(source);
        return;
    }
    // the line has no room for line breaks: a pasted script becomes a sequence of commands
    insertPlainText(ConsoleShell::joinPastedLines(source->text()));
}

void ConsoleInputEdit::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        updateHeight();
    else if (event->type() == QEvent::PaletteChange)
        updateColorsForPalette();
}

void ConsoleInputEdit::updateHeight()
{
    const int height = fontMetrics().lineSpacing() + static_cast<int>(2 * document()->documentMargin()) + 2 * frameWidth() + 2;
    setFixedHeight(height);
}

void ConsoleInputEdit::updateColorsForPalette()
{
    highlighter_->setDarkBackground(palette().base().color().lightness() < 128);
}
