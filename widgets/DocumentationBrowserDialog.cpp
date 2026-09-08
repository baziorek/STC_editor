#include "DocumentationBrowserDialog.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QLineEdit>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

DocumentationBrowserDialog::DocumentationBrowserDialog(const QUrl& initialUrl, const QString& windowTitle,
                                                       QWidget* parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(windowTitle);
    resize(1100, 800);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* browser = new QWebEngineView(this);
    auto* navigation = new QToolBar(this);
    navigation->addAction(browser->pageAction(QWebEnginePage::Back));
    navigation->addAction(browser->pageAction(QWebEnginePage::Forward));
    navigation->addAction(browser->pageAction(QWebEnginePage::Reload));
    navigation->addSeparator();

    auto* addressBar = new QLineEdit(navigation);
    addressBar->setMinimumWidth(400);
    addressBar->setClearButtonEnabled(true);
    addressBar->setPlaceholderText(tr("Current address"));
    navigation->addWidget(addressBar);

    auto* copyAddress = new QToolButton(navigation);
    copyAddress->setAutoRaise(true);
    copyAddress->setIcon(QIcon::fromTheme("edit-copy"));
    copyAddress->setText(tr("Copy address"));
    copyAddress->setToolTip(tr("Copy current address"));
    navigation->addWidget(copyAddress);

    auto* openExternally = new QToolButton(navigation);
    openExternally->setAutoRaise(true);
    openExternally->setIcon(QIcon::fromTheme("internet-web-browser"));
    openExternally->setText(tr("Open in browser"));
    openExternally->setToolTip(tr("Open current address in the system browser"));
    navigation->addWidget(openExternally);

    layout->addWidget(navigation);
    layout->addWidget(browser);

    const auto updateAddressBar = [addressBar](const QUrl& url) {
        addressBar->setText(url.toDisplayString(QUrl::FullyDecoded));
    };
    connect(browser, &QWebEngineView::urlChanged, this, updateAddressBar);
    connect(addressBar, &QLineEdit::returnPressed, this, [browser, addressBar]() {
        const QUrl url = QUrl::fromUserInput(addressBar->text().trimmed());
        if (url.isValid() && !url.isEmpty())
            browser->setUrl(url);
    });
    connect(copyAddress, &QToolButton::clicked, this, [browser]() {
        QGuiApplication::clipboard()->setText(browser->url().toString(QUrl::FullyEncoded));
    });
    connect(openExternally, &QToolButton::clicked, this, [browser]() {
        const QUrl url = browser->url();
        if (!url.isEmpty())
            QDesktopServices::openUrl(url);
    });

    browser->setUrl(initialUrl);
}
