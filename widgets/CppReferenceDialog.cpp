#include "CppReferenceDialog.h"

#include <QToolBar>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace
{
QUrl cppReferenceSearchUrl(const QString& symbol)
{
    QUrl url("https://duckduckgo.com/");
    QUrlQuery query;
    query.addQueryItem("q", QString("site:cppreference.com \"%1\" C++").arg(symbol));
    url.setQuery(query);
    return url;
}
} // namespace

CppReferenceDialog::CppReferenceDialog(const QString& symbol, QWidget* parent)
    : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("C++ documentation: %1").arg(symbol));
    resize(1100, 800);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* browser = new QWebEngineView(this);
    auto* navigation = new QToolBar(this);
    navigation->addAction(browser->pageAction(QWebEnginePage::Back));
    navigation->addAction(browser->pageAction(QWebEnginePage::Forward));
    navigation->addAction(browser->pageAction(QWebEnginePage::Reload));

    layout->addWidget(navigation);
    layout->addWidget(browser);

    browser->setUrl(cppReferenceSearchUrl(symbol));
}
