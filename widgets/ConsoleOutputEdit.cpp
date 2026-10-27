#include "ConsoleOutputEdit.h"

#include <QAction>
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

const CommandBlockData* commandData(const QTextBlock& block)
{
    return dynamic_cast<const CommandBlockData*>(block.userData());
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

    const QFontMetrics metrics(font());
    painter.setFont(font());
    const int digit = metrics.horizontalAdvance(QLatin1Char('9'));
    const int linesWidth = digit * kLineCountDigits;
    const int durationWidth = digit * kDurationChars;

    QColor normal = palette().color(QPalette::Text);
    normal.setAlpha(150);
    const QColor active = palette().color(QPalette::Highlight);

    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            const CommandBlockData* data = commandData(block);
            if (data && data->info->showStats)
            {
                const ConsoleCommandInfo& info = *data->info;
                const QRect linesRect(kPadding, top, linesWidth, metrics.height());
                const QRect durationRect(kPadding + linesWidth + kGap, top, durationWidth, metrics.height());
                painter.setPen(normal);
                painter.drawText(linesRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(qMin(info.lineCount, 999999)));
                painter.setPen(info.running ? active : normal);
                painter.drawText(durationRect, Qt::AlignRight | Qt::AlignVCenter, ConsoleShell::formatDuration(info.durationMs));
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
