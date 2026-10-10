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
#include <QMouseEvent>
#include <QChildEvent>
#include "StcPreview.h"
#include "HtmlSourceDialog.h"


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

    // The mouse events of the page go to the widget inside of the view (it can be created later), not to the view
    webView.installEventFilter(this);
    for (QObject *child : webView.children())
    {
        installClickFilter(child);
    }
}

void StcPreviewWidget::installClickFilter(QObject *renderWidget)
{
    if (renderWidget->isWidgetType())
    {
        renderWidget->installEventFilter(this);
    }
}

bool StcPreviewWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == &webView)
    {
        if (event->type() == QEvent::ChildAdded)
        {
            installClickFilter(static_cast<QChildEvent *>(event)->child());
        }
        return false;
    }

    const bool press = event->type() == QEvent::MouseButtonPress;
    if (press || event->type() == QEvent::MouseButtonRelease)
    {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            // a click is a press and a release in (almost) the same place; a drag is selecting the text
            constexpr int kClickTolerance = 5;
            if (press)
            {
                mousePressedAt = mouse->position().toPoint();
            }
            else if ((mouse->position().toPoint() - mousePressedAt).manhattanLength() < kClickTolerance)
            {
                findSourceOfClick(mouse->position().toPoint());
            }
        }
    }
    return false; // the page still gets the event
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

    QAction *clickSync = menu->addAction(tr("Move the editor cursor to the place clicked in the preview"));
    clickSync->setCheckable(true);
    clickSync->setChecked(clickSyncEnabled);
    connect(clickSync, &QAction::toggled, this, &StcPreviewWidget::setClickSyncEnabled);

    QAction *copyHtml = menu->addAction(tr("Copy preview HTML to clipboard"));
    connect(copyHtml, &QAction::triggered, this, &StcPreviewWidget::copyRenderedHtmlToClipboard);

    // The browser's own "View page source" opens a new browser window, which this widget cannot show, so it did nothing:
    // our window with the HTML is there instead
    QAction *showSource = new QAction(tr("View HTML source of the preview"), menu);
    connect(showSource, &QAction::triggered, this, &StcPreviewWidget::showHtmlSource);
    if (QAction *browserViewSource = webView.pageAction(QWebEnginePage::ViewSource); menu->actions().contains(browserViewSource))
    {
        menu->insertAction(browserViewSource, showSource);
        menu->removeAction(browserViewSource);
    }
    else
    {
        menu->addAction(showSource);
    }

    menu->popup(webView.mapToGlobal(position));
}

void StcPreviewWidget::showHtmlSource()
{
    if (latestHtml.isEmpty())
    {
        QToolTip::showText(QCursor::pos(),
                           tr("Nothing to show yet - open the STC preview (F6), log in to cpp0x.pl and let it render some text."),
                           this);
        return;
    }

    if (!htmlSourceDialog)
    {
        htmlSourceDialog = new HtmlSourceDialog(window());
        connect(htmlSourceDialog, &HtmlSourceDialog::refreshRequested, this, [this]() {
            htmlSourceDialog->setHtml(latestHtml);
        });
    }

    htmlSourceDialog->setHtml(latestHtml);
    htmlSourceDialog->show();
    htmlSourceDialog->raise();
    htmlSourceDialog->activateWindow();
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
        ++renderGeneration;
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

void StcPreviewWidget::setClickSyncEnabled(bool enabled)
{
    if (clickSyncEnabled == enabled)
    {
        return;
    }

    clickSyncEnabled = enabled;
    emit clickSyncEnabledChanged(enabled);
}

void StcPreviewWidget::findSourceOfClick(const QPoint &clickedAt)
{
    if (!clickSyncEnabled || !isInitialized || syncMap.isEmpty())
    {
        return;
    }

    // The place in the page: the character (of a text node) under the point. A click which ends a selection of text
    // is not a click for us (the user is copying), so it is when nothing is selected.
    const QPointF pagePoint = QPointF(clickedAt) / webView.zoomFactor();
    const QString js = QString(R"(
        (function(x, y) {
            const selection = window.getSelection();
            if (selection && selection.toString().length > 0) {
                return null;
            }

            let node = null;
            let offset = 0;
            if (document.caretPositionFromPoint) {
                const caret = document.caretPositionFromPoint(x, y);
                if (caret) {
                    node = caret.offsetNode;
                    offset = caret.offset;
                }
            } else if (document.caretRangeFromPoint) {
                const range = document.caretRangeFromPoint(x, y);
                if (range) {
                    node = range.startContainer;
                    offset = range.startOffset;
                }
            }
            if (!node) {
                return null;
            }

            if (node.nodeType !== Node.TEXT_NODE) {
                // the click is on an element, not on a text: the first text in it (or after the place in it)
                const from = node.childNodes[offset] || node;
                if (from.nodeType === Node.TEXT_NODE) {
                    node = from;
                } else {
                    const walker = document.createTreeWalker(from, NodeFilter.SHOW_TEXT);
                    if (!walker.nextNode()) {
                        return null;
                    }
                    node = walker.currentNode;
                }
                offset = 0;
            }

            const textNodes = window.__stcTextNodes;
            const chunk = textNodes ? textNodes.indexOf(node) : -1;
            return chunk < 0 ? null : [chunk, offset];
        })(%1, %2);
    )").arg(pagePoint.x()).arg(pagePoint.y());

    const int generation = renderGeneration;
    webView.page()->runJavaScript(js, [this, generation](const QVariant &answer) {
        const QVariantList place = answer.toList();
        if (place.size() != 2 || generation != renderGeneration)
        {
            return; // not a click on the text, or the page was rendered again in the meantime
        }

        const int position = syncMap.sourcePositionAt({place[0].toInt(), place[1].toInt()});
        if (position >= 0)
        {
            emit sourcePositionClicked(position);
        }
    });
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
