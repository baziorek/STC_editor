#include "HtmlSourceDialog.h"

#include <QCheckBox>
#include <QClipboard>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QCodeEditor>      // from QCodeEditor
#include <QSyntaxStyle>     // from QCodeEditor
#include <QXMLHighlighter>  // from QCodeEditor
#include "utils/HtmlPrettyPrinter.h"

HtmlSourceDialog::HtmlSourceDialog(QWidget *parent) : QDialog(parent), editor(new QCodeEditor(this)), statistics(new QLabel(this))
{
    setWindowTitle(tr("HTML source of the preview"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1000, 750);

    editor->setReadOnly(true);
    editor->setSyntaxStyle(QSyntaxStyle::defaultStyle());
    editor->setHighlighter(new QXMLHighlighter);
    editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor->setLineWrapMode(QTextEdit::NoWrap);

    auto *wrapLines = new QCheckBox(tr("Wrap lines"), this);
    connect(wrapLines, &QCheckBox::toggled, this, [this](bool wrap) {
        editor->setLineWrapMode(wrap ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    });

    auto *refresh = new QPushButton(tr("Refresh"), this);
    refresh->setToolTip(tr("Shows the HTML of the text rendered most recently"));
    connect(refresh, &QPushButton::clicked, this, &HtmlSourceDialog::refreshRequested);

    auto *copy = new QPushButton(tr("Copy"), this);
    copy->setToolTip(tr("Copies the formatted HTML shown here"));
    connect(copy, &QPushButton::clicked, this, &HtmlSourceDialog::copyAll);

    auto *close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(statistics, 1);
    buttons->addWidget(wrapLines);
    buttons->addWidget(refresh);
    buttons->addWidget(copy);
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(editor, 1);
    layout->addLayout(buttons);
}

void HtmlSourceDialog::setHtml(const QString &html)
{
    const int scrolled = editor->verticalScrollBar()->value();

    const QString formatted = HtmlPrettyPrinter::prettyPrint(html);
    editor->setPlainText(formatted);
    editor->verticalScrollBar()->setValue(scrolled);

    statistics->setText(tr("%1 lines, %2 characters of HTML from the server").arg(editor->document()->blockCount()).arg(html.size()));
}

void HtmlSourceDialog::copyAll()
{
    QGuiApplication::clipboard()->setText(editor->toPlainText());
}
