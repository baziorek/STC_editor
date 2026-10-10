#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QPoint>
#include <optional>
#include "utils/PreviewSyncMap.h"

/**
 * @class StcPreviewWidget
 * @brief A widget that provides real-time HTML preview rendering for STC-formatted (Smart Text Converter) text using cpp0x.pl backend.
 *
 * This widget allows a client application to:
 * - Authenticate with the cpp0x.pl service
 * - Send user-provided text with STC tags to the cpp0x.pl/STC endpoint
 * - Retrieve and display the resulting HTML in a QWebEngineView
 *
 * ### Usage:
 * 1. Call `login(username, password)` to authenticate the user. This is required before using the preview.
 * 2. Once initialized, call `updateText(stcText)` to trigger server-side rendering and display the result.
 * 3. Connect to the `htmlReady(const QString &html)` signal to react when new content is rendered.
 *
 * ### Efficiency:
 * Text updates are debounced: if multiple updates are queued during an active request,
 * only the latest pending text will be sent once the current request finishes.
 *
 * ### Styling:
 * On successful login and token retrieval, the CSS used by cpp0x.pl is fetched
 * and embedded directly into the HTML preview for consistent appearance.
 *
 * ### Copying the rendered HTML:
 * `copyRenderedHtmlToClipboard()` (also available from the preview's context menu) puts the HTML fragment
 * returned by cpp0x.pl for the current text on the clipboard - handy for pasting it into a bug report
 * or a conversation. It is the same fragment that is placed inside the preview's `#Preview` container.
 *
 * ### Scrolling together with the editor:
 * The HTML from cpp0x.pl does not say which part of the source is which part of the page, so after each render
 * the text of the page is compared with the text sent (PreviewSync::SyncMap). `scrollToSourceLine()` then scrolls
 * the page to the place which shows the given line of the source - the user scrolls only the editor and the preview follows.
 * It can be switched off with a checkbox in the context menu of the preview (`setScrollSyncEnabled()`); on by default.
 *
 * ### Going from the preview to the source:
 * A click in the preview (not a selection of text with the mouse) emits `sourcePositionClicked()` with the position in the
 * source of the place which was clicked, so the editor can put its cursor there. Another checkbox in the context menu
 * switches it off (`setClickSyncEnabled()`); on by default.
 *
 * ### Statistics:
 * For debugging or diagnostics, you can access request statistics via `getStats()`.
 *
 * ### Disclaimer and Permission:
 * The use of the cpp0x.pl server for rendering is based on direct permission from the user `pekfos`,
 * granted in the following forum thread: https://cpp0x.pl/forum/temat/?id=4756&p=675
 *
 * > "Co za różnica jakiej używasz \"przeglądarki\". Po prostu wysyłaj rozsądną ilość requestów."
 *
 * ### Notes:
 * - If the user is not logged in or the preview has not been initialized, calling `updateText` will throw.
 * - This widget is designed to be embedded in applications like editors or documentation tools.
 * - It requires an active internet connection.
 */
class StcPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    struct Stats
    {
        int requestCount = 0;
        qint64 bytesSent = 0;
        qint64 bytesReceived = 0;
    };

    explicit StcPreviewWidget(QWidget *parent = nullptr);

    void login(const QString &username, const QString &password);
    void updateText(const QString &text);

    const Stats &getStats() const
    {
        return stats;
    }

    bool isPreviewInitialized() const
    {
        return isInitialized;
    }

    /// The HTML fragment cpp0x.pl returned for the most recently rendered text (empty until the first render)
    const QString &renderedHtml() const
    {
        return latestHtml;
    }

    /// Puts renderedHtml() on the clipboard and shows a short tooltip with the result.
    /// @return false when there is nothing to copy yet
    bool copyRenderedHtmlToClipboard();

    /// Scrolls the preview to the place of the page which shows the given part of the source: the `line` (zero-based)
    /// which is at the top of the editor, `fraction` (0..1) of the way through it. When the editor is scrolled
    /// to its end, the preview goes to its end too. The place is remembered, so it is also used after the next render.
    /// Does nothing visible while the synchronization is switched off.
    void scrollToSourceLine(int line, double fraction, bool atEndOfDocument);

    bool isScrollSyncEnabled() const
    {
        return scrollSyncEnabled;
    }
    void setScrollSyncEnabled(bool enabled);

    bool isClickSyncEnabled() const
    {
        return clickSyncEnabled;
    }
    void setClickSyncEnabled(bool enabled);

signals:
    void htmlReady(const QString &html);

    void loginFailed(const QString &message);
    void loginSucceeded();
    void scrollSyncEnabledChanged(bool enabled);
    void clickSyncEnabledChanged(bool enabled);

    /// The place of the preview which was clicked is this place of the source: the number of the character (as a
    /// position of a QTextCursor in the text which was sent with updateText())
    void sourcePositionClicked(int position);

protected:
    void updateStatsLabel();
    void fetchStcSecurityToken();
    void loadCssAndInitialize();
    void sendTextRequest(const QString &text);
    void showRenderedHtml(const QString &html);
    void showPreviewContextMenu(const QPoint &position);
    void scheduleTextUpdate();
    void applyScrollSync();
    void forgetLastScroll();
    void installClickFilter(QObject *renderWidget);
    void findSourceOfClick(const QPoint &clickedAt);
    bool eventFilter(QObject *watched, QEvent *event) override;
    static QString toJsStringLiteral(const QString &text);

    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

    QUrl makeUrl(const QString &path) const
    {
        return baseUrl.resolved(QUrl(path));
    }

private:
    const QUrl baseUrl{"https://cpp0x.pl"};

    QWebEngineView webView;
    QNetworkAccessManager network;
    QString securityToken;
    QString baseCss;

    QString latestHtml;

    QString pendingText;
    QString lastSentText;
    bool requestInProgress = false;
    bool isInitialized = false;
    bool hasPendingUpdate = false;

    Stats stats;

    /// Where the editor is, as the last call of scrollToSourceLine() said
    struct EditorViewportTop
    {
        int line = 0;
        double fraction = 0.0;
        bool atEndOfDocument = false;
    };

    bool scrollSyncEnabled = true;
    bool clickSyncEnabled = true;
    QPoint mousePressedAt;
    int renderGeneration = 0;                                  ///< counts the maps built, to drop an answer which is about an older page
    std::optional<EditorViewportTop> editorTop;
    PreviewSync::SyncMap syncMap;                              ///< lines of the source <-> text nodes of the page, after the last render
    std::optional<PreviewSync::TextPosition> lastScrolledTo;   ///< not to ask the page for the same scroll again and again
    bool lastScrolledToEnd = false;
};
