#include "MinimapWidget.h"

#include <QApplication>
#include <QFontInfo>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include "CodeEditor.h"

MinimapWidget::MinimapWidget(QWidget* parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setFocusPolicy(Qt::NoFocus);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setCursor(Qt::ArrowCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(100);

    contentTimer_ = new QTimer(this);
    contentTimer_->setSingleShot(true);
    contentTimer_->setInterval(400);
    connect(contentTimer_, &QTimer::timeout, this, &MinimapWidget::updateContent);
}

void MinimapWidget::setEditor(CodeEditor* editor)
{
    if (editor_ == editor)
    {
        updateContent();
        syncScrollFromEditor();
        return;
    }

    if (editor_)
        editor_->disconnect(this);

    editor_ = editor;

    if (!editor_)
    {
        clear();
        return;
    }

    setBaseFont(editor->font());

    connect(editor_, &QPlainTextEdit::textChanged, this, &MinimapWidget::scheduleContentUpdate);
    connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged, this, &MinimapWidget::syncScrollFromEditor);
    connect(editor_->verticalScrollBar(), &QScrollBar::rangeChanged, this, &MinimapWidget::syncScrollFromEditor);

    updateContent();
    syncScrollFromEditor();
}

void MinimapWidget::setBaseFont(const QFont& font)
{
    baseFont_ = font;

    QFont miniFont = font;
    int pixelSize = miniFont.pixelSize();
    if (pixelSize <= 0)
        pixelSize = QFontInfo(miniFont).pixelSize();
    // about a fifth of the editor font: still a few pixels, the whole document fits the side
    miniFont.setPixelSize(qMax(2, pixelSize / 5));
    miniFont.setBold(false);
    miniFont.setItalic(false);
    setFont(miniFont);
}

void MinimapWidget::scheduleContentUpdate()
{
    contentTimer_->start();
}

void MinimapWidget::updateContent()
{
    if (!editor_)
    {
        clear();
        return;
    }
    setPlainText(editor_->toPlainText());
}

void MinimapWidget::syncScrollFromEditor()
{
    if (!editor_ || syncingScroll_)
        return;

    auto* editorBar = editor_->verticalScrollBar();
    auto* miniBar = verticalScrollBar();
    const int editorMax = editorBar->maximum();
    const int miniMax = miniBar->maximum();
    if (editorMax <= 0 || miniMax <= 0)
        return;

    syncingScroll_ = true;
    miniBar->setValue(qRound(editorBar->value() * double(miniMax) / editorMax));
    syncingScroll_ = false;
}

void MinimapWidget::scrollEditorToViewportY(int y)
{
    if (!editor_)
        return;

    const int height = viewport()->height();
    if (height <= 1)
        return;

    const double fraction = qBound(0.0, double(y) / height, 1.0);
    auto* bar = editor_->verticalScrollBar();
    // the clicked place lands in the middle of the editor viewport
    const int value = qRound(fraction * bar->maximum()) - editor_->viewport()->height() / 2;
    bar->setValue(qBound(bar->minimum(), value, bar->maximum()));
}

void MinimapWidget::wheelEvent(QWheelEvent* event)
{
    if (editor_)
    {
        // the wheel scrolls the editor, not the minimap itself
        QWheelEvent forwarded(event->position(), event->globalPosition(),
                              event->pixelDelta(), event->angleDelta(), event->buttons(),
                              event->modifiers(), event->phase(), event->inverted());
        QApplication::sendEvent(editor_, &forwarded);
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void MinimapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        dragging_ = true;
        scrollEditorToViewportY(event->position().y());
    }
}

void MinimapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_)
        scrollEditorToViewportY(event->position().y());
}

void MinimapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        dragging_ = false;
}
