#include <QLabel>
#include <QVBoxLayout>
#include <QNetworkReply>
#include <QNetworkReply>
#include <QEnterEvent>
#include <QToolTip>
#include <QCursor>
#include "StcPreview.h"


namespace
{
QString humanReadableBytes(qint64 bytes)
{
    constexpr const char *units[] = {"B", "KB", "MB", "GB"};
    double size = bytes;
    int unit = 0;
    while (size >= 1024.0 && unit < 3)
    {
        size /= 1024.0;
        ++unit;
    }
    return QString::number(size, 'f', 1) + " " + units[unit];
}
} // namespace


StcPreviewWidget::StcPreviewWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(&webView);

    // Display initial empty preview container
    webView.setHtml("<html><body><div id='Preview'></div></body></html>", makeUrl("/"));

    setFocusPolicy(Qt::NoFocus);
    webView.setFocusPolicy(Qt::NoFocus);
}

void StcPreviewWidget::login(const QString &username, const QString &password) {
    // Step 1: Load login page to extract CSRF security token
    QNetworkRequest tokenRequest(makeUrl("/logowanie/"));
    QNetworkReply *tokenReply = network.get(tokenRequest);

    connect(tokenReply, &QNetworkReply::finished, this, [=, this]() {
        QString html = tokenReply->readAll();
        tokenReply->deleteLater();

        QRegularExpression re("name=\"SecurityToken\" value=\"([a-z0-9]+)\"");
        QRegularExpressionMatch match = re.match(html);
        if (!match.hasMatch())
        {
            emit loginFailed("Security token not found on login page.");
            return;
        }

        QString loginToken = match.captured(1);
        QUrlQuery postData;
        postData.addQueryItem("SecurityToken", loginToken);
        postData.addQueryItem("UserPanel_Login", username);
        postData.addQueryItem("UserPanel_Password", password);
        postData.addQueryItem("noscroll", "1");
        postData.addQueryItem("post", "[account] logon panel");
        postData.addQueryItem("ajax", "ddt");

        QNetworkRequest loginReq(makeUrl("/logowanie/"));
        loginReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        QNetworkReply *reply = network.post(loginReq, postData.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this, [=, this]() {
            QString resp = reply->readAll();
            reply->deleteLater();

            if (! resp.contains("Autoryzacja zakończona powodzeniem"))
            {
                emit loginFailed("Login failed: authorization string not found.");
                return;
            }

            fetchStcSecurityToken();
        });
    });
}

void StcPreviewWidget::fetchStcSecurityToken()
{
    // Step 2: Retrieve STC token from the STC panel page
    QNetworkRequest req(makeUrl("/stc/"));
    QNetworkReply *reply = network.get(req);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        QString html = reply->readAll();
        reply->deleteLater();

        QRegularExpression re("name=\"SecurityToken\" value=\"([a-z0-9]+)\"");
        QRegularExpressionMatch match = re.match(html);
        if (!match.hasMatch()) {
            emit loginFailed("STC security token not found.");
            return;
        }

        securityToken = match.captured(1);
        loadCssAndInitialize();
    });
}

void StcPreviewWidget::loadCssAndInitialize()
{
    // Step 3: Load and embed the main stylesheet
    QNetworkRequest req(makeUrl("/release.css"));
    QNetworkReply *reply = network.get(req);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        baseCss = reply->readAll();
        reply->deleteLater();

        QString html = QString(R"(
            <html><head><style>%1</style></head>
            <body>
                <div class="Layout" id="PanelPage">
                    <div class="Preview" id="Preview"></div>
                </div>
            </body></html>
        )").arg(baseCss);

        webView.setHtml(html, makeUrl("/"));

        connect(&webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
            if (ok) {
                isInitialized = true;
                scheduleTextUpdate();

                emit loginSucceeded();
            } else {
                emit loginFailed("Failed to load preview HTML into WebView.");
            }
        });
    });
}

void StcPreviewWidget::updateText(const QString &text)
{
    if (isHidden() || parentWidget()->isHidden())
    {
        return;
    }

    if (!isInitialized || securityToken.isEmpty())
    {
        emit loginFailed("Preview not ready. Not authenticated or initialized.");
        return;
    }

    pendingText = text;
    hasPendingUpdate = true;
    scheduleTextUpdate();
}

void StcPreviewWidget::scheduleTextUpdate()
{
    if (requestInProgress)
    {
        return;
    }

    if (!hasPendingUpdate || pendingText == lastSentText)
    {
        return;
    }

    hasPendingUpdate = false;
    requestInProgress = true;
    lastSentText = pendingText;
    sendTextRequest(pendingText);
}

void StcPreviewWidget::sendTextRequest(const QString &text)
{
    QUrlQuery postData;
    postData.addQueryItem("stc", text);
    postData.addQueryItem("ajax", "ddt");
    postData.addQueryItem("SecurityToken", securityToken);

    QNetworkRequest req(makeUrl("/stc/"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QByteArray payload = postData.toString(QUrl::FullyEncoded).toUtf8();

    stats.bytesSent += payload.size();
    stats.requestCount++;

    QNetworkReply *reply = network.post(req, payload);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        requestInProgress = false;

        QByteArray response = reply->readAll();
        stats.bytesReceived += response.size();

        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(response);
        QString html = doc["html"].toString();

        QString js = QString(R"(
            (function() {
                let container = document.getElementById("Preview");
                if (container) {
                    container.innerHTML = '%1';
                }
            })();
        )").arg(escapeHtmlToJsString(html).replace("$", "\\$"));

        webView.page()->runJavaScript(js);
        emit htmlReady(html);

        if (hasPendingUpdate && pendingText != lastSentText)
        {
            scheduleTextUpdate();
        }
    });
}

QString StcPreviewWidget::escapeHtmlToJsString(const QString &html)
{
    QJsonArray arr;
    arr.append(html);
    QString wrapped = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    return wrapped.mid(2, wrapped.length() - 4); // Strip leading [" and trailing "]
}

void StcPreviewWidget::updateStatsLabel()
{
    QString text = QString("Requests: %1 | Sent: %2 | Received: %3")
        .arg(stats.requestCount)
        .arg(humanReadableBytes(stats.bytesSent))
        .arg(humanReadableBytes(stats.bytesReceived));

    // Show the tooltip at the top of the widget (under mouse or at fixed point)
    QPoint globalPos = mapToGlobal(QPoint(width() / 2, 0));
    QToolTip::showText(globalPos, text, this);
}

void StcPreviewWidget::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    updateStatsLabel();
}

void StcPreviewWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    QToolTip::hideText();
}
