#include <QRegularExpression>
#include <QStack>
#include <QUrl>
#include <QTextBlock>
#include "BreadcrumbTextBrowser.h"
#include "codeeditor.h"
#include "widgets/FilteredTagTableWidget.h"


static const QSet<QString> selfClosingTags = { "img", "a" };


BreadcrumbTextBrowser::BreadcrumbTextBrowser(QWidget* parent)
    : QTextBrowser(parent)
{
    setFrameStyle(QFrame::NoFrame);
    connect(this, &QTextBrowser::anchorClicked, this, &BreadcrumbTextBrowser::onAnchorClicked);
}

void BreadcrumbTextBrowser::setTextEditor(CodeEditor* editor)
{
    textEditor = editor;
}

void BreadcrumbTextBrowser::setHeaderTable(FilteredTagTableWidget* table)
{
    headerTable = table;
}

void BreadcrumbTextBrowser::onAnchorClicked(const QUrl& link)
{
    bool ok;
    int pos = link.toString().toInt(&ok);
    if (!ok || !textEditor)
        return;

    QTextCursor cursor = textEditor->textCursor();
    cursor.setPosition(pos);
    textEditor->setTextCursor(cursor);
    textEditor->ensureCursorVisible();

    int line = cursor.block().blockNumber() + 1;
    int offset = cursor.positionInBlock();

    emit goToLineAndOffsetRequested(line, offset);
}

void BreadcrumbTextBrowser::updateBreadcrumb(const QTextCursor& cursor)
{
    setHtml(buildBreadcrumbHtml(cursor));
}

QString BreadcrumbTextBrowser::buildBreadcrumbHtml(const QTextCursor& cursor)
{
    if (!textEditor || !headerTable)
        return {};

    int pos = cursor.position();

    const QString text = textEditor->document()->toPlainText();
    static QRegularExpression tagOpen(R"(\[([a-z0-9]+)(?:\s+[^\]]+)?\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression tagClose(R"(\[/([a-z0-9]+)\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression headerRegex(R"(\[(h[1-6])\](.*?)\[/\1\])",
                                          QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    QMap<int, QPair<QString, int>> headers;
    QRegularExpressionMatchIterator it = headerRegex.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        if (match.capturedStart() >= pos)
            break;

        const QString headerTag = match.captured(1).toLower();
        const QString content = match.captured(2).trimmed();
        const int level = headerTag.mid(1).toInt();
        if (textEditor->isInsideCode(match.capturedStart()))
            continue;

        auto it = headers.begin();
        while (it != headers.end()) {
            if (it.key() >= level)
                it = headers.erase(it);
            else
                ++it;
        }

        int tagOpenLen = QString("[%1]").arg(headerTag).length();
        headers[level] = qMakePair(QString("%1: %2").arg(headerTag.toUpper(), content),
                                   match.capturedStart() + tagOpenLen);
    }

    // track all outer context tags like [div], [run], etc.
    QStack<QPair<QString, int>> tagStack;
    QMap<QString, int> openPos;

    int i = 0;
    while (i < pos)
    {
        auto mOpen = tagOpen.match(text, i);
        auto mClose = tagClose.match(text, i);

        int oPos = mOpen.hasMatch() ? mOpen.capturedStart() : -1;
        int cPos = mClose.hasMatch() ? mClose.capturedStart() : -1;

        if (oPos != -1 && (cPos == -1 || oPos < cPos) && oPos < pos)
        {
            QString tag = mOpen.captured(1).toLower();
            if (!tag.startsWith("h") && !textEditor->isInsideCode(oPos) && !selfClosingTags.contains(tag))
            {
                tagStack.push({ tag, mOpen.capturedEnd() });
                openPos[tag] = oPos;
            }

            i = mOpen.capturedEnd();
        }
        else if (cPos != -1 && cPos < pos)
        {
            QString tag = mClose.captured(1).toLower();
            if (openPos.contains(tag) && !textEditor->isInsideCode(openPos[tag]))
            {
                int idx = tagStack.size() - 1;
                while (idx >= 0 && tagStack[idx].first != tag)
                    --idx;
                if (idx >= 0)
                    tagStack.remove(idx);
            }
            i = mClose.capturedEnd();
        }
        else
        {
            break;
        }
    }

    QStringList parts;

    for (int level : headers.keys())
        parts << tagLink(headers[level].second, headers[level].first);

    for (const auto& [tag, tagPos] : tagStack)
        parts << tagLink(tagPos, tag.toUpper());

    if (auto codeInfo = textEditor->getCodeTagAtPosition(pos)) {
        parts << tagLink(codeInfo->position, codeInfo->tag.toUpper());
    }

    return parts.join(" &gt; ");
}

QString BreadcrumbTextBrowser::tagLink(int pos, const QString& label)
{
    QTextCursor cursor(textEditor->document());
    cursor.setPosition(pos);
    const int line = cursor.block().blockNumber() + 1;
    const int col = cursor.positionInBlock() + 1;

    return QString(R"(<a href="%1" title="Line: %2, Column: %3">%4</a>)")
        .arg(pos)
        .arg(line)
        .arg(col)
        .arg(label.toHtmlEscaped());
}
