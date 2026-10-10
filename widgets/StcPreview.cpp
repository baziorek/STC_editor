#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
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

    // The page takes the focus when it is clicked, so that Ctrl+C copies what is selected in the page, not in the editor
    setFocusPolicy(Qt::NoFocus);
    webView.setFocusPolicy(Qt::ClickFocus);

    webView.setContextMenuPolicy(Qt::CustomContextMenu);
    connect(&webView, &QWidget::customContextMenuRequested, this, &StcPreviewWidget::showPreviewContextMenu);
}

void StcPreviewWidget::showPreviewContextMenu(const QPoint &position)
{
    // The standard entries (copy, select all, ...) exist only while a context menu request from the page is being handled
    QMenu *menu = webView.lastContextMenuRequest() ? webView.createStandardContextMenu() : new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (!menu->isEmpty())
        menu->addSeparator();

    QAction *syncScroll = menu->addAction(tr("Synchronize scrolling with the editor"));
    syncScroll->setCheckable(true);
    syncScroll->setChecked(scrollSyncEnabled);
    connect(syncScroll, &QAction::toggled, this, &StcPreviewWidget::setScrollSyncEnabled);

    QAction *copyHtml = menu->addAction(tr("Copy preview HTML to clipboard"));
    connect(copyHtml, &QAction::triggered, this, &StcPreviewWidget::copyRenderedHtmlToClipboard);

    menu->popup(webView.mapToGlobal(position));
}

bool StcPreviewWidget::copyRenderedHtmlToClipboard()
{
    if (latestHtml.isEmpty())
    {
        QToolTip::showText(QCursor::pos(),
                           tr("Nothing to copy yet - open the STC preview (F6), log in to cpp0x.pl and let it render some text."),
                           this);
        return false;
    }

    QGuiApplication::clipboard()->setText(latestHtml);
    QToolTip::showText(QCursor::pos(),
                       tr("Preview HTML copied to clipboard (%1 characters)").arg(latestHtml.size()),
                       this);
    return true;
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
    auto formEncode = [](const QString &s) -> QByteArray {
        QByteArray encoded = QUrl::toPercentEncoding(s);
        return encoded;
    };

    QByteArray payload;
    payload += "stc=" + formEncode(text);
    payload += "&ajax=" + formEncode("ddt");
    payload += "&SecurityToken=" + formEncode(securityToken);

    QNetworkRequest req(makeUrl("/stc/"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    stats.bytesSent += payload.size();
    stats.requestCount++;

    QNetworkReply *reply = network.post(req, payload);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        requestInProgress = false;

        QByteArray response = reply->readAll();
        stats.bytesReceived += response.size();

        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(response);
        showRenderedHtml(doc["html"].toString());

        if (hasPendingUpdate && pendingText != lastSentText)
        {
            scheduleTextUpdate();
        }
    });
}

void StcPreviewWidget::showRenderedHtml(const QString &html)
{
    latestHtml = html;

    // The page is replaced and, in the same script, its text nodes are collected (and remembered in the page,
    // to be found by their numbers when scrolling) - they are what the lines of the source are matched with.
    QString js = QString(R"(
        (function() {
            let container = document.getElementById("Preview");
            if (!container) {
                return [];
            }
            container.innerHTML = %1;

            const textNodes = [];
            const walker = document.createTreeWalker(container, NodeFilter.SHOW_TEXT);
            while (walker.nextNode()) {
                textNodes.push(walker.currentNode);
            }
            window.__stcTextNodes = textNodes;
            return textNodes.map(node => node.data);
        })();
    )").arg(toJsStringLiteral(html));

    const QString renderedSource = lastSentText; // the response is for the text sent last (one request at a time)
    webView.page()->runJavaScript(js, [this, renderedSource](const QVariant &textNodes) {
        syncMap = PreviewSync::SyncMap(renderedSource, textNodes.toStringList());
        forgetLastScroll();
        applyScrollSync();
    });
    emit htmlReady(html);
}

void StcPreviewWidget::scrollToSourceLine(int line, double fraction, bool atEndOfDocument)
{
    editorTop = EditorViewportTop{line, fraction, atEndOfDocument};
    applyScrollSync();
}

void StcPreviewWidget::setScrollSyncEnabled(bool enabled)
{
    if (scrollSyncEnabled == enabled)
    {
        return;
    }

    scrollSyncEnabled = enabled;
    forgetLastScroll();
    emit scrollSyncEnabledChanged(enabled);

    applyScrollSync();
}

void StcPreviewWidget::forgetLastScroll()
{
    lastScrolledTo.reset();
    lastScrolledToEnd = false;
}

void StcPreviewWidget::applyScrollSync()
{
    if (!scrollSyncEnabled || !editorTop || !isInitialized)
    {
        return;
    }

    if (editorTop->atEndOfDocument)
    {
        if (lastScrolledToEnd)
        {
            return;
        }
        forgetLastScroll();
        lastScrolledToEnd = true;

        webView.page()->runJavaScript(R"(
            (function() {
                const scroller = document.scrollingElement || document.documentElement;
                scroller.scrollTo({top: scroller.scrollHeight, behavior: 'instant'});
            })();
        )");
        return;
    }

    const PreviewSync::TextPosition position = syncMap.positionForLine(editorTop->line, editorTop->fraction);
    if (!position.isValid() || (!lastScrolledToEnd && lastScrolledTo == position))
    {
        return;
    }
    forgetLastScroll();
    lastScrolledTo = position;

    // The place of the character in the page decides, so it is right whatever the width of the preview is
    QString js = QString(R"(
        (function(chunk, offset) {
            const textNodes = window.__stcTextNodes;
            const node = textNodes && textNodes[chunk];
            if (!node || !node.isConnected) {
                return;
            }
            const start = Math.min(offset, node.length);
            const range = document.createRange();
            range.setStart(node, start);
            range.setEnd(node, Math.min(start + 1, node.length));

            let rect = range.getBoundingClientRect();
            if (rect.width === 0 && rect.height === 0 && node.parentElement) {
                rect = node.parentElement.getBoundingClientRect();
            }
            const scroller = document.scrollingElement || document.documentElement;
            scroller.scrollTo({top: Math.max(0, rect.top + scroller.scrollTop - 2), behavior: 'instant'});
        })(%1, %2);
    )").arg(position.chunk).arg(position.offset);

    webView.page()->runJavaScript(js);
}

QString StcPreviewWidget::toJsStringLiteral(const QString &text)
{
    // A JSON string (with its double quotes) is a valid JavaScript string literal whatever the text contains.
    // Stripping the quotes and wrapping the rest in '...' instead breaks on every apostrophe in the HTML
    // (e.g. Bjarne's, '\n' in code, single-quoted attributes): the script does not even parse and the preview stays stale.
    QJsonArray arr;
    arr.append(text);
    const QString wrapped = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    return wrapped.mid(1, wrapped.length() - 2); // strip the surrounding [ and ]
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
