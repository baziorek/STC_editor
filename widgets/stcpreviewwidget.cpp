#include <stdexcept>
#include <QVBoxLayout>
#include <QNetworkReply>
#include <QNetworkReply>
#include "stcpreviewwidget.h"


StcPreviewWidget::StcPreviewWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&webView);

    // Display initial empty preview container
    webView.setHtml("<html><body><div id='Preview'></div></body></html>", makeUrl("/"));
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
        if (!match.hasMatch()) {
            throw std::runtime_error("Security token not found on login page.");
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

            if (! resp.contains("Autoryzacja zakończona powodzeniem")) {
                throw std::runtime_error("Login failed: authorization string not found.");
            }

            fetchStcSecurityToken();
        });
    });
}

void StcPreviewWidget::fetchStcSecurityToken() {
    // Step 2: Retrieve STC token from the STC panel page
    QNetworkRequest req(makeUrl("/stc/"));
    QNetworkReply *reply = network.get(req);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        QString html = reply->readAll();
        reply->deleteLater();

        QRegularExpression re("name=\"SecurityToken\" value=\"([a-z0-9]+)\"");
        QRegularExpressionMatch match = re.match(html);
        if (!match.hasMatch()) {
            throw std::runtime_error("STC security token not found.");
        }

        securityToken = match.captured(1);
        loadCssAndInitialize();
    });
}

void StcPreviewWidget::loadCssAndInitialize() {
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
        isInitialized = true;
        scheduleTextUpdate();
    });
}

void StcPreviewWidget::updateText(const QString &text)
{
    if (!isInitialized || securityToken.isEmpty())
        throw std::runtime_error("Preview not ready. Not authenticated or initialized.");

    pendingText = text;
    hasPendingUpdate = true;
    scheduleTextUpdate();
}

void StcPreviewWidget::scheduleTextUpdate() {
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

void StcPreviewWidget::sendTextRequest(const QString &text) {
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
                    container.innerHTML = `%1`;
                }
            })();
        )").arg(escapeHtmlToJsString(html));

        webView.page()->runJavaScript(js);
        emit htmlReady(html);

        if (hasPendingUpdate && pendingText != lastSentText) {
            scheduleTextUpdate();
        }
    });
}

QString StcPreviewWidget::escapeHtmlToJsString(const QString &html) {
    QJsonArray arr;
    arr.append(html);
    QString wrapped = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    return wrapped.mid(2, wrapped.length() - 4); // Strip leading [" and trailing "]
}

void StcPreviewWidget::printStats() const {
    qDebug() << "Requests:" << stats.requestCount;
    qDebug() << "Bytes sent:" << stats.bytesSent;
    qDebug() << "Bytes received:" << stats.bytesReceived;
}
