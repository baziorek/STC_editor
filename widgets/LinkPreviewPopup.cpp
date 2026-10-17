#include "LinkPreviewPopup.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QPalette>
#include <QScreen>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <algorithm>

namespace
{
constexpr int kHeaderMargin = 8;
} // namespace

LinkPreviewPopup::LinkPreviewPopup(QWidget* parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    setFixedSize(kSize);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(1, 1, 1, 1);
    layout_->setSpacing(0);

    header_ = new QLabel(this);
    header_->setTextFormat(Qt::RichText);
    header_->setContentsMargins(kHeaderMargin, 4, kHeaderMargin, 4);
    header_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); // a long text is cut, it must not widen the window
    layout_->addWidget(header_);
}

LinkPreviewPopup::~LinkPreviewPopup()
{
    // A profile must outlive its pages: view first, then the page, the profile at the end
    delete webView_;
    delete page_;
    delete profile_;
}

void LinkPreviewPopup::createWebView()
{
    profile_ = new QWebEngineProfile(this); // without a storage name: off the record, nothing is kept on the disk
    page_ = new QWebEnginePage(profile_, this);
    webView_ = new QWebEngineView(this);
    webView_->setPage(page_);
    webView_->setFocusPolicy(Qt::NoFocus);
    webView_->setZoomFactor(kZoomFactor);
    restrictPage();

    connect(page_, &QWebEnginePage::titleChanged, this, [this](const QString& title) {
        title_ = title;
        refreshHeader();
    });
    connect(page_, &QWebEnginePage::loadStarted, this, [this] {
        loading_ = true;
        failed_ = false;
        refreshHeader();
    });
    connect(page_, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (currentUrl_.isEmpty())
            return; // the blank page loaded by hidePreview()
        loading_ = false;
        failed_ = !ok;
        refreshHeader();
    });

    layout_->addWidget(webView_, 1);
}

void LinkPreviewPopup::restrictPage()
{
    // A preview must not be a way to download anything, open windows, play sound or ask for permissions.
    // (A request for a permission which nobody grants is denied; a window of the page which nobody creates is not opened.)
    connect(profile_, &QWebEngineProfile::downloadRequested, this, [](QWebEngineDownloadRequest* download) {
        download->cancel();
    });

    QWebEngineSettings* settings = page_->settings();
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
    settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, true);
    settings->setAttribute(QWebEngineSettings::ShowScrollBars, false);
    page_->setAudioMuted(true);
}

void LinkPreviewPopup::showPreview(const QUrl& url, const QPoint& globalPosition)
{
    if (webView_ == nullptr)
        createWebView();

    if (url != currentUrl_)
    {
        currentUrl_ = url;
        title_.clear();
        loading_ = true;
        failed_ = false;
        refreshHeader();
        page_->load(url);
    }

    move(positionNear(globalPosition));
    show();
}

void LinkPreviewPopup::hidePreview()
{
    hide();
    if (page_ == nullptr || currentUrl_.isEmpty())
        return;

    currentUrl_.clear();
    page_->load(QUrl(QStringLiteral("about:blank"))); // stops the page which was shown: loading, scripts, sound
}

void LinkPreviewPopup::refreshHeader()
{
    QString first;
    if (failed_)
        first = tr("Could not load the page");
    else if (!title_.isEmpty() && title_ != currentUrl_.toString())
        first = title_; // while a page loads, QtWebEngine reports its address as the title
    else if (loading_)
        first = tr("Loading…");
    else
        first = currentUrl_.host();

    const QFontMetrics metrics(header_->font());
    const int textWidth = kSize.width() - 2 * kHeaderMargin - 2;
    first = metrics.elidedText(first, Qt::ElideRight, textWidth);
    const QString address = metrics.elidedText(currentUrl_.toDisplayString(), Qt::ElideMiddle, textWidth);
    const QString grey = palette().color(QPalette::PlaceholderText).name();

    header_->setText(QStringLiteral("<b>%1</b><br/><span style=\"color:%2\">%3</span>")
                         .arg(first.toHtmlEscaped(), grey, address.toHtmlEscaped()));
}

QPoint LinkPreviewPopup::positionNear(const QPoint& globalPosition) const
{
    constexpr int kOffsetRight = 12;
    constexpr int kOffsetBelow = 24;
    constexpr int kGapAbove = 12;

    const QScreen* screen = QGuiApplication::screenAt(globalPosition);
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();

    int x = std::min(globalPosition.x() + kOffsetRight, available.right() - width() + 1);
    int y = globalPosition.y() + kOffsetBelow;
    if (y + height() > available.bottom() + 1)
        y = globalPosition.y() - height() - kGapAbove;

    return {std::max(x, available.left()), std::max(y, available.top())};
}
