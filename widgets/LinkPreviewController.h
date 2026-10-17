#pragma once

#include <functional>
#include <optional>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include "utils/LinkPreviewTarget.h"

class LinkPreviewPopup;
class QMouseEvent;
class QPlainTextEdit;
class QTextCursor;

/** @brief Shows a preview of the web page when the mouse stays over a link in the editor.
 *
 * It only watches the editor (event filter), so the editor itself needs no code for it. A link is the
 * `[a href="..."]` tag, in plain text mode also an address written in the text. The preview appears after the mouse
 * has rested on the link for a moment (kHoverDelayMs), and disappears when the mouse leaves the link or the editor,
 * on a click, scrolling, typing or when the window loses the focus. Only http and https pages are previewed.
 *
 * The user can switch it off (it loads pages from the Internet, like a browser does): the choice is kept in the settings. */
class LinkPreviewController : public QObject
{
    Q_OBJECT

public:
    /// Tells whether an address written in the text (not only the tag) is a link in the current syntax mode
    using BareUrlsAreLinks = std::function<bool()>;

    LinkPreviewController(QPlainTextEdit* editor, BareUrlsAreLinks bareUrlsAreLinks);

    bool isEnabled() const { return enabled_; }
    /// Remembered in the settings; switching off hides the preview which is shown
    void setEnabled(bool enabled);

    /// The page which would be previewed for the link at the place of the cursor, regardless of the switch
    std::optional<stc::links::PreviewTarget> targetAt(const QTextCursor& cursor) const;

    static constexpr int kHoverDelayMs = 500;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onMouseMoved(const QMouseEvent* event);
    void showPreviewOfHoveredLink();
    void hidePreview();

    QPlainTextEdit* editor_;
    BareUrlsAreLinks bareUrlsAreLinks_;
    bool enabled_ = true;

    QPointer<LinkPreviewPopup> popup_; // created at the first preview; owned by the editor
    QTimer hoverTimer_;
    QUrl hoveredUrl_;                  // the link the mouse rests on, empty when there is none
    QPoint hoverGlobalPosition_;
};
