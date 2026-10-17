#include "LinkPreviewController.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include "widgets/LinkPreviewPopup.h"

namespace
{
constexpr const char kEnabledSetting[] = "linkPreview/enabled";
} // namespace

LinkPreviewController::LinkPreviewController(QPlainTextEdit* editor, BareUrlsAreLinks bareUrlsAreLinks)
    : QObject(editor), editor_(editor), bareUrlsAreLinks_(std::move(bareUrlsAreLinks))
{
    enabled_ = QSettings().value(kEnabledSetting, true).toBool();

    hoverTimer_.setSingleShot(true);
    hoverTimer_.setInterval(kHoverDelayMs);
    connect(&hoverTimer_, &QTimer::timeout, this, &LinkPreviewController::showPreviewOfHoveredLink);

    viewport_ = editor_->viewport();
    viewport_->setMouseTracking(true); // the mouse is watched without any button pressed
    viewport_->installEventFilter(this);
    editor_->installEventFilter(this);
}

void LinkPreviewController::setEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;

    enabled_ = enabled;
    QSettings().setValue(kEnabledSetting, enabled);
    if (!enabled_)
        hidePreview();
}

std::optional<stc::links::PreviewTarget> LinkPreviewController::targetAt(const QTextCursor& cursor) const
{
    const QTextBlock block = cursor.block();
    const bool includeBareUrls = bareUrlsAreLinks_ && bareUrlsAreLinks_();
    return stc::links::previewTargetAt(block.text(), cursor.position() - block.position(), includeBareUrls);
}

bool LinkPreviewController::eventFilter(QObject* watched, QEvent* event)
{
    // Only pointers are compared here, no method of the editor is called: while the editor is destroyed it has already
    // lost its QPlainTextEdit part (it is only a QWidget), but it still sends events (FocusOut, Hide) through this filter.
    if (watched == viewport_)
    {
        switch (event->type())
        {
        case QEvent::MouseMove:
            onMouseMoved(static_cast<QMouseEvent*>(event));
            break;
        case QEvent::Leave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::ContextMenu:
        case QEvent::Wheel:
            hidePreview();
            break;
        default:
            break;
        }
    }
    else if (watched == editor_)
    {
        switch (event->type())
        {
        case QEvent::KeyPress:
        case QEvent::FocusOut:
        case QEvent::WindowDeactivate:
        case QEvent::Hide:
            hidePreview();
            break;
        default:
            break;
        }
    }
    return false; // only watching: the editor handles all the events as usual
}

void LinkPreviewController::onMouseMoved(const QMouseEvent* event)
{
    const bool selectingOrDragging = event->buttons() != Qt::NoButton;
    if (!enabled_ || selectingOrDragging)
    {
        hidePreview();
        return;
    }

    const auto target = targetAt(editor_->cursorForPosition(event->position().toPoint()));
    if (!target)
    {
        hidePreview();
        return;
    }
    if (target->url == hoveredUrl_)
        return; // still on the same link: the preview stays where it is

    hidePreview(); // moved from one link to another
    hoveredUrl_ = target->url;
    hoverGlobalPosition_ = event->globalPosition().toPoint();
    hoverTimer_.start();
}

void LinkPreviewController::showPreviewOfHoveredLink()
{
    if (!enabled_ || hoveredUrl_.isEmpty() || !editor_->window()->isActiveWindow())
        return;

    if (popup_.isNull())
        popup_ = new LinkPreviewPopup(editor_);
    popup_->showPreview(hoveredUrl_, hoverGlobalPosition_);
}

void LinkPreviewController::hidePreview()
{
    hoverTimer_.stop();
    hoveredUrl_.clear();
    if (!popup_.isNull())
        popup_->hidePreview();
}
