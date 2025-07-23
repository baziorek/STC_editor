#include <QRegularExpression>
#include <QStack>
#include <QUrl>
#include <QTextBlock>
#include "BreadcrumbTextBrowser.h"
#include "codeeditor.h"
#include "widgets/FilteredTagTableWidget.h"


static const QSet<QString> selfClosingTags2Ignore = { "img", "a" };


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

    const int cursorPos = cursor.position();
    const QTextDocument* doc = textEditor->document();

    // Przygotowanie do budowania breadcrumb
    QStringList breadcrumbParts;

    // --- 1. Pobieramy nagłówki z cachedHeaders ---
    QMap<int, QPair<QString, int>> activeHeaders; // level -> (label, position)
    int startPosForTagScan = 0;

    const QList<FilteredTagTableWidget::HeaderInfo>& headers = headerTable->getCachedHeaders();

    for (const auto& header : headers)
    {
        if (header.startPos >= cursorPos)
            break;

        if (textEditor->isInsideCode(header.startPos))
            continue;

        const QString tag = header.tagName.toLower(); // h1, h2...
        if (!tag.startsWith('h'))
            continue;

        bool ok = false;
        int level = tag.mid(1).toInt(&ok);
        if (!ok)
            continue;

        // Usuń wszystkie nagłówki o większym lub równym poziomie
        auto it = activeHeaders.begin();
        while (it != activeHeaders.end())
        {
            if (it.key() >= level)
                it = activeHeaders.erase(it);
            else
                ++it;
        }

        const QString label = QString("%1: %2").arg(tag.toUpper(), header.textInside.trimmed());
        activeHeaders[level] = qMakePair(label, header.startPos);
        startPosForTagScan = header.endPos;
    }

    // Dodajemy nagłówki do breadcrumb
    for (int level : activeHeaders.keys())
        breadcrumbParts << tagLink(activeHeaders[level].second, activeHeaders[level].first);

    // --- 2. Parsowanie tagów kontekstowych między ostatnim nagłówkiem a kursorem ---
    static QRegularExpression tagOpen(R"(\[([a-z0-9]+)(?:\s+[^\]]+)?\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression tagClose(R"(\[/([a-z0-9]+)\])", QRegularExpression::CaseInsensitiveOption);

    QStack<QPair<QString, int>> tagStack;
    QMap<QString, QStack<int>> openTagPositions;

    QTextBlock block = doc->findBlock(startPosForTagScan);
    while (block.isValid())
    {
        const QString lineText = block.text();
        int blockStart = block.position();

        int limit = block.length();
        if (block.contains(cursorPos))
            limit = cursorPos - blockStart;

        int index = 0;
        while (index < limit)
        {
            QRegularExpressionMatch mOpen = tagOpen.match(lineText, index);
            QRegularExpressionMatch mClose = tagClose.match(lineText, index);

            int oPos = mOpen.hasMatch() ? mOpen.capturedStart() : -1;
            int cPos = mClose.hasMatch() ? mClose.capturedStart() : -1;

            if (oPos != -1 && (cPos == -1 || oPos < cPos) && oPos < limit)
            {
                QString tag = mOpen.captured(1).toLower();
                int globalPos = blockStart + mOpen.capturedStart();

                if (!tag.startsWith('h') &&
                    !textEditor->isInsideCode(globalPos) &&
                    !selfClosingTags2Ignore.contains(tag))
                {
                    tagStack.push({ tag, globalPos });
                    openTagPositions[tag].push(globalPos);
                }

                index = mOpen.capturedEnd();
            }
            else if (cPos != -1 && cPos < limit)
            {
                QString tag = mClose.captured(1).toLower();
                int globalClosePos = blockStart + mClose.capturedStart();

                if (openTagPositions.contains(tag) &&
                    !openTagPositions[tag].isEmpty() &&
                    !textEditor->isInsideCode(openTagPositions[tag].top()))
                {
                    int openPos = openTagPositions[tag].pop();

                    // Usuń pasujące otwarcie z tagStack
                    for (int i = tagStack.size() - 1; i >= 0; --i)
                    {
                        if (tagStack[i].first == tag && tagStack[i].second == openPos)
                        {
                            tagStack.remove(i);
                            break;
                        }
                    }
                }

                index = mClose.capturedEnd();
            }
            else
            {
                break;
            }
        }

        if (block.contains(cursorPos))
            break;

        block = block.next();
    }

    for (const auto& [tag, tagPos] : tagStack)
        breadcrumbParts << tagLink(tagPos, tag.toUpper());

    // --- 3. Dodaj tag kodu jeśli jesteśmy w środku bloku kodu ---
    if (auto codeInfo = textEditor->getCodeTagAtPosition(cursorPos))
        breadcrumbParts << tagLink(codeInfo->position, codeInfo->tag.toUpper());

    return breadcrumbParts.join(" &gt; ");
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
