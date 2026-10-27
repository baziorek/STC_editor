#include "ConsoleOutputEdit.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QScrollBar>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextCursor>
#include <QToolTip>
#include <Qt>

#include "utils/ConsoleShell.h"

namespace
{
constexpr int kPadding = 4;
constexpr int kGap = 8;
constexpr int kLineCountDigits = 6;
constexpr int kDurationChars = 7; // "59m 59s"

/// Attached to the line of the command; the numbers are shared with the console, which updates them while it runs.
class CommandBlockData : public QTextBlockUserData
{
public:
    explicit CommandBlockData(std::shared_ptr<ConsoleCommandInfo> info) : info(std::move(info)) {}
    std::shared_ptr<ConsoleCommandInfo> info;
};

/// Attached to a line of the output of a command: its number (1, 2, 3, ...).
class LineNumberData : public QTextBlockUserData
{
public:
    explicit LineNumberData(int number) : number(number) {}
    int number;
};

/// Attached to a line which is not the output of the command (the status line, the typed line sent to the program).
class AnnotationData : public QTextBlockUserData
{
};

const CommandBlockData* commandData(const QTextBlock& block)
{
    return dynamic_cast<const CommandBlockData*>(block.userData());
}

/// The line is an empty last line of the document: the place for the next text.
bool isTrailingEmptyLine(const QTextBlock& block)
{
    return !block.next().isValid() && block.length() <= 1;
}
} // namespace

/// The margin on the left side of the output (the same idea as the line numbers of a code editor).
class ConsoleMargin : public QWidget
{
public:
    explicit ConsoleMargin(ConsoleOutputEdit* edit) : QWidget(edit), edit_(edit) {}
    QSize sizeHint() const override { return {edit_->marginWidth(), 0}; }

protected:
    void paintEvent(QPaintEvent* event) override { edit_->paintMargin(event); }
    void mouseDoubleClickEvent(QMouseEvent* event) override { edit_->marginDoubleClicked(static_cast<int>(event->position().y())); }
    void mousePressEvent(QMouseEvent* event) override { edit_->marginPressed(static_cast<int>(event->position().y())); }
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ToolTip)
        {
            const auto* help = static_cast<QHelpEvent*>(event);
            const QString text = edit_->marginToolTipAt(help->pos().y());
            if (text.isEmpty())
                QToolTip::hideText();
            else
                QToolTip::showText(help->globalPos(), text, this);
            return true;
        }
        return QWidget::event(event);
    }

private:
    ConsoleOutputEdit* edit_;
};

ConsoleOutputEdit::ConsoleOutputEdit(QWidget* parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    margin_ = new ConsoleMargin(this);
    connect(this, &QPlainTextEdit::updateRequest, this, &ConsoleOutputEdit::updateMarginArea);
    updateMarginWidth();
}

ConsoleOutputEdit::~ConsoleOutputEdit() = default;

void ConsoleOutputEdit::markCommandLine(const std::shared_ptr<ConsoleCommandInfo>& info)
{
    QTextBlock block = document()->lastBlock().previous();
    if (!block.isValid())
        return;
    block.setUserData(new CommandBlockData(info));

    if (!hasCommands_ && info->showStats)
    {
        hasCommands_ = true;
        updateMarginWidth();
    }
    refreshMargin();
}

void ConsoleOutputEdit::refreshMargin()
{
    margin_->update();
}

void ConsoleOutputEdit::numberOutputLines(const QTextBlock& firstNewBlock, ConsoleCommandInfo& info)
{
    for (QTextBlock block = firstNewBlock; block.isValid() && !isTrailingEmptyLine(block); block = block.next())
    {
        if (!block.userData())
            block.setUserData(new LineNumberData(info.nextLineNumber++));
    }
}

void ConsoleOutputEdit::markAnnotationLine()
{
    QTextBlock block = document()->lastBlock().previous();
    if (block.isValid())
        block.setUserData(new AnnotationData);
}

int ConsoleOutputEdit::lineNumberOf(const QTextBlock& block) const
{
    const auto* data = dynamic_cast<const LineNumberData*>(block.userData());
    return data ? data->number : 0;
}

QList<ConsoleCommandInfo> ConsoleOutputEdit::commandInfos() const
{
    QList<ConsoleCommandInfo> result;
    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next())
    {
        if (const CommandBlockData* data = commandData(block))
            result.append(*data->info);
    }
    return result;
}

// ------------------------------------------------------------------ the margin

int ConsoleOutputEdit::marginWidth() const
{
    if (!hasCommands_)
        return 0;
    const QFontMetrics metrics(font());
    const int digit = metrics.horizontalAdvance(QLatin1Char('9'));
    return kPadding + digit * kLineCountDigits + kGap + digit * kDurationChars + kPadding;
}

void ConsoleOutputEdit::updateMarginWidth()
{
    const int width = marginWidth();
    setViewportMargins(width, 0, 0, 0);
    margin_->setVisible(width > 0);
    margin_->setGeometry(QRect(contentsRect().left(), contentsRect().top(), width, contentsRect().height()));
}

void ConsoleOutputEdit::updateMarginArea(const QRect& rect, int dy)
{
    if (dy != 0)
        margin_->scroll(0, dy);
    else
        margin_->update(0, rect.y(), margin_->width(), rect.height());
}

void ConsoleOutputEdit::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    margin_->setGeometry(QRect(area.left(), area.top(), marginWidth(), area.height()));
}

void ConsoleOutputEdit::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange)
        updateMarginWidth();
}

void ConsoleOutputEdit::paintMargin(QPaintEvent* event)
{
    QPainter painter(margin_);
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(margin_->width() - 1, event->rect().top(), margin_->width() - 1, event->rect().bottom());

    // The duration of a command: to the left, in bold. The numbers of the lines of its output: to the right, small and dim.
    QFont durationFont = font();
    durationFont.setBold(true);
    QFont numberFont = font();
    numberFont.setPointSizeF(qMax(6.0, font().pointSizeF() * 0.9));
    const QFontMetrics metrics(font());
    const QFontMetrics numberMetrics(numberFont);
    const int digit = metrics.horizontalAdvance(QLatin1Char('9'));
    const int durationWidth = digit * kDurationChars;
    const int numbersRight = margin_->width() - kPadding - 2; // 2: the separator line

    QColor durationColor = palette().color(QPalette::Text);
    durationColor.setAlpha(210);
    const QColor runningColor = palette().color(QPalette::Highlight);
    QColor numberColor = palette().color(QPalette::Text);
    numberColor.setAlpha(110);

    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            if (const CommandBlockData* data = commandData(block); data && data->info->showStats)
            {
                const ConsoleCommandInfo& info = *data->info;
                painter.setFont(durationFont);
                painter.setPen(info.running ? runningColor : durationColor);
                painter.drawText(QRect(kPadding, top, durationWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                                 ConsoleShell::formatDuration(info.durationMs));
            }
            else if (const int number = lineNumberOf(block); number > 0)
            {
                painter.setFont(numberFont);
                painter.setPen(numberColor);
                painter.drawText(QRect(kPadding, top, numbersRight - kPadding, metrics.height()), Qt::AlignRight | Qt::AlignVCenter,
                                 QString::number(number));
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

QString ConsoleOutputEdit::marginToolTipAt(int y) const
{
    // the viewport starts at the same height as the margin
    const QTextCursor cursor = cursorForPosition(QPoint(0, y));
    const CommandBlockData* data = commandData(cursor.block());
    if (!data || !data->info->showStats)
        return {};
    const ConsoleCommandInfo& info = *data->info;
    return tr("%1 lines of output, %2%3\nstarted in %4")
        .arg(info.lineCount)
        .arg(ConsoleShell::formatDuration(info.durationMs), info.running ? tr(" (running)") : QString(),
             QDir::toNativeSeparators(info.workingDirectory));
}

// ------------------------------------------------------------------ paths

QString ConsoleOutputEdit::directoryOfCommandAt(const QTextBlock& block) const
{
    for (QTextBlock b = block; b.isValid(); b = b.previous())
    {
        if (const CommandBlockData* data = commandData(b))
            return data->info->workingDirectory;
    }
    return defaultDirectory_;
}

void ConsoleOutputEdit::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        const QTextCursor cursor = cursorForPosition(event->position().toPoint());
        const QTextBlock block = cursor.block();
        const QString directory = directoryOfCommandAt(block);

        for (const QString& candidate : ConsoleShell::pathCandidatesAt(block.text(), cursor.positionInBlock()))
        {
            const QString path = ConsoleShell::resolvePath(directory, candidate);
            if (QFileInfo::exists(path))
            {
                event->accept();
                emit pathActivated(path);
                return;
            }
        }
    }
    QPlainTextEdit::mouseDoubleClickEvent(event); // not a path: the word is selected as always
}

void ConsoleOutputEdit::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();
    QAction* clear = menu->addAction(tr("Clear the console"));
    connect(clear, &QAction::triggered, this, &ConsoleOutputEdit::clearRequested);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

// ------------------------------------------------------------------ jumps between the commands

QTextBlock ConsoleOutputEdit::commandBlockOf(QTextBlock block) const
{
    for (; block.isValid(); block = block.previous())
    {
        if (commandData(block))
            return block;
    }
    return {};
}

QTextBlock ConsoleOutputEdit::nextCommandBlock(QTextBlock block) const
{
    for (block = block.next(); block.isValid(); block = block.next())
    {
        if (commandData(block))
            return block;
    }
    return {};
}

void ConsoleOutputEdit::scrollBlockToTop(const QTextBlock& block)
{
    // the scroll bar counts lines; a few corrections are enough also when a long line is wrapped
    QScrollBar* bar = verticalScrollBar();
    const int lineHeight = qMax(1, QFontMetrics(font()).lineSpacing());
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const int y = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        if (y == 0)
            return;
        const int before = bar->value();
        bar->setValue(before + (y > 0 ? qMax(1, y / lineHeight) : qMin(-1, y / lineHeight)));
        if (bar->value() == before)
            return;
    }
}

void ConsoleOutputEdit::jumpToPreviousCommand()
{
    // the nearest command above the first visible line (when the window starts with a command, the one before it)
    for (QTextBlock block = firstVisibleBlock().previous(); block.isValid(); block = block.previous())
    {
        if (commandData(block))
        {
            scrollBlockToTop(block);
            return;
        }
    }
}

void ConsoleOutputEdit::jumpToNextCommand()
{
    const QTextBlock next = nextCommandBlock(firstVisibleBlock());
    if (next.isValid())
        scrollBlockToTop(next);
    else
        verticalScrollBar()->setValue(verticalScrollBar()->maximum()); // after the last one: the end (the output follows again)
}

// ------------------------------------------------------------------ selecting a command from the margin

QTextBlock ConsoleOutputEdit::blockAtMargin(int y) const
{
    const QTextBlock last = document()->lastBlock();
    const qreal end = blockBoundingGeometry(last).translated(contentOffset()).bottom();
    if (y >= end)
        return {}; // below the text
    return cursorForPosition(QPoint(0, y)).block();
}

void ConsoleOutputEdit::marginDoubleClicked(int y)
{
    selectCommand(blockAtMargin(y), /*withCommandLine=*/false);
    tripleClick_.start();
}

void ConsoleOutputEdit::marginPressed(int y)
{
    // Qt sends no event for the third click: it is a press soon after a double click
    if (tripleClick_.isValid() && tripleClick_.elapsed() <= QApplication::doubleClickInterval())
    {
        tripleClick_.invalidate();
        selectCommand(blockAtMargin(y), /*withCommandLine=*/true);
    }
}

void ConsoleOutputEdit::selectCommand(const QTextBlock& blockOfCommand, bool withCommandLine)
{
    const QTextBlock command = commandBlockOf(blockOfCommand);
    if (!command.isValid())
        return;

    // the output goes up to the next command, or to the end (without the empty line which waits for the next text)
    const QTextBlock next = nextCommandBlock(command);
    QTextBlock lastLine = next.isValid() ? next.previous() : document()->lastBlock();
    if (isTrailingEmptyLine(lastLine) && lastLine != command)
        lastLine = lastLine.previous();

    const QTextBlock firstLine = withCommandLine ? command : command.next();
    if (!firstLine.isValid() || firstLine.position() > lastLine.position())
        return; // a command without output

    QTextCursor cursor(document());
    cursor.setPosition(firstLine.position());
    cursor.setPosition(lastLine.position() + lastLine.length() - 1, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    setFocus(); // Ctrl+C copies it
}
