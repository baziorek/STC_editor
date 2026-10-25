#pragma once

#include <QFrame>
#include <QSize>
#include <QString>
#include <QUrl>
#include <QObject>
#include <QPoint>
#include <QtGlobal>

class QLabel;
class QVBoxLayout;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;

/** @brief A small window with a preview of the web page which the link under the mouse leads to.
 *
 * Bigger than the preview of an image, because it is a real page rendered by QtWebEngine (scaled down by
 * kZoomFactor, so more of it fits). The window never takes the focus and never takes the mouse: it is only to look at,
 * the owner (LinkPreviewController) hides it as soon as the mouse leaves the link.
 *
 * The page is loaded in a private, off-the-record profile (no cookies, history or cache on the disk, no sign-ins of the
 * user), with plug-ins off, no pop-up windows, no downloads, no sound and no permissions (camera, location...) granted.
 * The web view is created at the first preview: it is expensive and many users never hover a link. */
class LinkPreviewPopup : public QFrame
{
    Q_OBJECT

public:
    /// `parent` only owns the window (it is a separate tool-tip-like window, not a child inside of `parent`)
    explicit LinkPreviewPopup(QWidget* parent);
    ~LinkPreviewPopup() override;

    static constexpr QSize kSize{520, 360};
    static constexpr qreal kZoomFactor = 0.6;

    /// Starts loading `url` (unless it is already shown) and shows the window near `globalPosition` - below the
    /// place, or above it when there is no room, always inside of the screen.
    void showPreview(const QUrl& url, const QPoint& globalPosition);
    /// Hides the window and stops the page (loading, scripts, sound): there is nothing left running in the background.
    void hidePreview();

    QUrl currentUrl() const { return currentUrl_; }

private:
    void createWebView();
    void restrictPage();
    void refreshHeader();
    QPoint positionNear(const QPoint& globalPosition) const;

    QVBoxLayout* layout_ = nullptr;
    QLabel* header_ = nullptr;
    QWebEngineProfile* profile_ = nullptr;
    QWebEnginePage* page_ = nullptr;
    QWebEngineView* webView_ = nullptr;

    QUrl currentUrl_;
    QString title_;
    bool loading_ = false;
    bool failed_ = false;
};
