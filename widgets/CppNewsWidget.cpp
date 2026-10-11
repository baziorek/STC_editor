#include "CppNewsWidget.h"

#include <QDesktopServices>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QShowEvent>
#include <QHideEvent>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
enum ItemRole
{
    ThreadIdRole = Qt::UserRole
};

QList<int> intsFrom(const QVariant& value)
{
    QList<int> result;
    for (const QVariant& item : value.toList())
        result.append(item.toInt());
    return result;
}

QVariantList variantsFrom(const QList<int>& ints)
{
    QVariantList result;
    for (const int value : ints)
        result.append(value);
    return result;
}
} // namespace

CppNewsWidget::CppNewsWidget(QWidget* parent)
    : QWidget(parent)
{
    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);

    list = new QListWidget(this);
    list->setToolTip(tr("Double-click a thread to open it on the forum"));
    connect(list, &QListWidget::itemDoubleClicked, this, &CppNewsWidget::onItemDoubleClicked);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(statusLabel);
    layout->addWidget(list);

    checkTimer.setInterval(checkIntervalMs);
    connect(&checkTimer, &QTimer::timeout, this, &CppNewsWidget::checkNow);

    loadState();
}

QString CppNewsWidget::feedUrl()
{
    return QStringLiteral("https://cpp0x.pl/xml/");
}

void CppNewsWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // checked at once when it appears, then while it stays visible
    checkNow();
    checkTimer.start();
}

void CppNewsWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    checkTimer.stop();
}

void CppNewsWidget::checkNow()
{
    if (checking)
        return;
    checking = true;

    QNetworkRequest request{QUrl(feedUrl())};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STC_editor"));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setTransferTimeout(20 * 1000);

    QNetworkReply* reply = network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] { onReplyFinished(reply); });
    statusLabel->setText(tr("Checking the news..."));
}

void CppNewsWidget::onReplyFinished(QNetworkReply* reply)
{
    checking = false;

    if (reply->error() != QNetworkReply::NoError)
    {
        // the last news stay on the list
        statusLabel->setText(tr("Could not check the news: %1").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    const NewsFeed feed = parseNewsFeed(body);
    if (!feed.wellFormed)
    {
        // not XML at all (a page of the site, an error text): the start of the answer is in the tooltip to see what it is
        statusLabel->setText(tr("The answer is not an XML feed"));
        statusLabel->setToolTip(QString::fromUtf8(body.left(300)));
        return;
    }
    if (feed.items.isEmpty())
    {
        QString shape = feed.structure;
        if (!feed.itemShape.isEmpty())
            shape += QStringLiteral(" | ") + feed.itemShape;
        statusLabel->setText(tr("No news found in the feed. Structure: %1").arg(shape));
        statusLabel->setToolTip(QString());
        return;
    }

    const QList<NewsItem> fresh = feed.items;
    readState.update(fresh);
    items = fresh;
    showItems();
    saveState();

    statusLabel->setToolTip(QString());
    statusLabel->setText(tr("Checked at %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))));
}

void CppNewsWidget::showItems()
{
    list->clear();
    for (const NewsItem& news : items)
    {
        auto* item = new QListWidgetItem(news.title);
        item->setData(ThreadIdRole, news.threadId);
        item->setToolTip(readState.isUnread(news.threadId) ? tr("New thread") : QString());
        updateItemFont(item);
        list->addItem(item);
    }
}

void CppNewsWidget::updateItemFont(QListWidgetItem* item) const
{
    QFont font = list->font();
    font.setBold(readState.isUnread(item->data(ThreadIdRole).toInt()));
    item->setFont(font);
}

void CppNewsWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item)
        return;

    const int threadId = item->data(ThreadIdRole).toInt();
    readState.markRead(threadId);
    saveState();
    updateItemFont(item);
    item->setToolTip(QString());

    QDesktopServices::openUrl(QUrl(forumThreadUrl(threadId)));
}

void CppNewsWidget::loadState()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("CppNews"));
    const bool baseline = settings.value(QStringLiteral("baseline"), false).toBool();
    const QList<int> known = intsFrom(settings.value(QStringLiteral("known")));
    const QList<int> unread = intsFrom(settings.value(QStringLiteral("unread")));
    settings.endGroup();

    readState.restore(baseline, known, unread);
}

void CppNewsWidget::saveState()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("CppNews"));
    settings.setValue(QStringLiteral("baseline"), readState.hasBaseline());
    settings.setValue(QStringLiteral("known"), variantsFrom(readState.knownThreads()));
    settings.setValue(QStringLiteral("unread"), variantsFrom(readState.unreadThreads()));
    settings.endGroup();
}
