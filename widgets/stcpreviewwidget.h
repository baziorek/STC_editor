#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QRegularExpression>

class QLabel;

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
class StcPreviewWidget : public QWidget {
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

signals:
    void htmlReady(const QString &html);

    void loginFailed(const QString &message);
    void loginSucceeded();

private:
    void updateStatsLabel();
    void fetchStcSecurityToken();
    void loadCssAndInitialize();
    void sendTextRequest(const QString &text);
    void scheduleTextUpdate();
    QString escapeHtmlToJsString(const QString &html);

    QUrl makeUrl(const QString &path) const
    {
        return baseUrl.resolved(QUrl(path));
    }

    const QUrl baseUrl{"https://cpp0x.pl"};

    QWebEngineView webView;
    QNetworkAccessManager network;
    QString securityToken;
    QString baseCss;

    QString pendingText;
    QString lastSentText;
    bool requestInProgress = false;
    bool isInitialized = false;
    bool hasPendingUpdate = false;

    Stats stats;
    QLabel *statsLabel = nullptr;
};
