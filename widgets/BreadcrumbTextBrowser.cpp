#include <QRegularExpression>
#include <QStack>
#include <QUrl>
#include <QTextBlock>
#include "BreadcrumbTextBrowser.h"
#include "CodeEditor.h"
#include "widgets/FilteredTagTableWidget.h"


static const QSet<QString> selfClosingTags2Ignore = { "img", "a" };


BreadcrumbTextBrowser::BreadcrumbTextBrowser(QWidget* parent)
    : QTextBrowser(parent)
{
    setFrameStyle(QFrame::NoFrame);
    connect(this, &QTextBrowser::anchorClicked, this, &BreadcrumbTextBrowser::onAnchorClicked);
}

void BreadcrumbTextBrowser::setTextEditor(CodeEditor* newEditor)
{
    if (textEditor)
    {
        disconnect(textEditor, &QPlainTextEdit::cursorPositionChanged, this, &BreadcrumbTextBrowser::onCursorPositionChanged);
        disconnect(this, &BreadcrumbTextBrowser::goToLineAndOffsetRequested, textEditor, &CodeEditor::goToLineAndOffset);
    }

    textEditor = newEditor;

    if (textEditor)
    {
        connect(textEditor, &QPlainTextEdit::cursorPositionChanged, this, &BreadcrumbTextBrowser::onCursorPositionChanged);
        connect(this, &BreadcrumbTextBrowser::goToLineAndOffsetRequested, textEditor, &CodeEditor::goToLineAndOffset);

        updateBreadcrumb(textEditor->textCursor());
    }
}
void BreadcrumbTextBrowser::onCursorPositionChanged()
{
    if (textEditor)
        updateBreadcrumb(textEditor->textCursor());
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

    const int pos = cursor.position();
    const QTextDocument* doc = textEditor->document();

    QStringList breadcrumbParts;

    // Step 1: Collect headers from FilteredTagTableWidget
    QMap<int, QPair<QString, int>> headers;
    int startScanPos = 0;
    extractHeadersBeforePosition(pos, headers, startScanPos);

    for (int level : headers.keys())
        breadcrumbParts << tagLink(headers[level].second, headers[level].first);

    // Step 2: Parse context tags from last header to cursor position
    const auto tagStack = collectContextTags(doc, startScanPos, pos);
    for (const auto& [tag, tagPos] : tagStack)
        breadcrumbParts << tagLink(tagPos, tag.toUpper());

    // Step 3: Add current code tag if inside
    if (auto codeInfo = textEditor->getCodeTagAtPosition(pos))
        breadcrumbParts << tagLink(codeInfo->position, codeInfo->tag.toUpper());

    return breadcrumbParts.join(" &gt; ");
}
void BreadcrumbTextBrowser::extractHeadersBeforePosition(int cursorPos, QMap<int, QPair<QString, int>>& headers, int& outStartScanPos) const
{
    const auto& cached = headerTable->getCachedHeaders();

    for (const auto& header : cached)
    {
        if (header.startPos >= cursorPos)
            break;

        if (textEditor->isInsideCode(header.startPos))
            continue;

        const QString tag = header.tagName.toLower();
        if (!tag.startsWith('h'))
            continue;

        bool ok = false;
        int level = tag.mid(1).toInt(&ok);
        if (!ok)
            continue;

        auto it = headers.begin();
        while (it != headers.end())
        {
            if (it.key() >= level)
                it = headers.erase(it);
            else
                ++it;
        }

        const QString label = QString("%1: %2").arg(tag.toUpper(), header.textInside.trimmed());
        headers[level] = qMakePair(label, header.startPos);
        outStartScanPos = header.endPos;
    }
}

QStack<QPair<QString, int>> BreadcrumbTextBrowser::collectContextTags(const QTextDocument* doc, int startPos, int cursorPos) const
{
    static QRegularExpression tagOpen(R"(\[([a-z0-9]+)(?:\s+[^\]]+)?\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression tagClose(R"(\[/([a-z0-9]+)\])", QRegularExpression::CaseInsensitiveOption);

    QStack<QPair<QString, int>> tagStack;
    QMap<QString, QStack<int>> openTagPositions;

    QTextBlock block = doc->findBlock(startPos);

    while (block.isValid())
    {
        const QString lineText = block.text();
        const int blockStart = block.position();
        int limit = block.length();
        if (block.contains(cursorPos))
            limit = cursorPos - blockStart;

        int index = 0;
        while (index < limit)
        {
            auto mOpen = tagOpen.match(lineText, index);
            auto mClose = tagClose.match(lineText, index);

            int oPos = mOpen.hasMatch() ? mOpen.capturedStart() : -1;
            int cPos = mClose.hasMatch() ? mClose.capturedStart() : -1;

            if (oPos != -1 && (cPos == -1 || oPos < cPos) && oPos < limit)
            {
                const QString tag = mOpen.captured(1).toLower();
                const int globalPos = blockStart + mOpen.capturedStart();

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
                const QString tag = mClose.captured(1).toLower();

                if (openTagPositions.contains(tag) &&
                    !openTagPositions[tag].isEmpty() &&
                    !textEditor->isInsideCode(openTagPositions[tag].top()))
                {
                    const int openPos = openTagPositions[tag].pop();

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

    return tagStack;
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
