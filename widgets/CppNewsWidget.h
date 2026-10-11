#pragma once

#include <QList>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QWidget>

#include "utils/CppNews.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QNetworkReply;

/// The news of cpp0x.pl (the feed at https://cpp0x.pl/xml/): the threads of the forum, the new ones in bold.
/// The feed is checked when the widget is shown and then every minute while it is visible. A double click opens
/// the thread on the forum and the thread is no longer new.
class CppNewsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CppNewsWidget(QWidget* parent = nullptr);

    static QString feedUrl();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void checkNow();
    void onReplyFinished(QNetworkReply* reply);
    void showItems();
    void updateItemFont(QListWidgetItem* item) const;
    void onItemDoubleClicked(QListWidgetItem* item);

    void loadState();
    void saveState();

    static constexpr int checkIntervalMs = 60 * 1000;

    QNetworkAccessManager network;
    QTimer checkTimer;
    QLabel* statusLabel = nullptr;
    QListWidget* list = nullptr;
    NewsReadState readState;
    QList<NewsItem> items;
    bool checking = false;
};
