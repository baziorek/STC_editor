/** @file CodeEditorFolding.cpp
 * @brief Folding in CodeEditor: collapsing the section of a header or a block of code into its first line.
 *
 * The lines are not removed, they are only hidden (QTextBlock::setVisible), the way Qt Creator does it with the same
 * QPlainTextEdit: the document stays intact, so saving, compiling a block, the edit history, the diff with the original
 * file and the preview see the whole text. Which lines are hidden is always computed from scratch from two things:
 * the regions found in the text (stcfold::findRegions) and the list of the folded ones (their first lines, kept as
 * QTextCursors so they follow the lines when the text above changes). A region which disappears from the text
 * (somebody deleted its closing tag) just stops hiding anything. */
#include "CodeEditor.h"

#include <QAbstractTextDocumentLayout>
#include <QMenu>
#include <QPainter>
#include <QPlainTextDocumentLayout>
#include <QShortcut>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>

namespace
{
constexpr int foldingUpdateDelayMs = 150; // typing is not interrupted by scanning the document on every key
} // namespace

void CodeEditor::setupFolding()
{
    foldingTimer_ = new QTimer(this);
    foldingTimer_->setSingleShot(true);
    foldingTimer_->setInterval(foldingUpdateDelayMs);
    connect(foldingTimer_, &QTimer::timeout, this, &CodeEditor::updateFolding);

    connect(document(), &QTextDocument::contentsChanged, this, &CodeEditor::scheduleFoldingUpdate);
    connect(this, &CodeEditor::syntaxModeChanged, this, &CodeEditor::updateFolding);

    // Only when the editor has the focus (not window-wide): Alt+Arrows are used by the text fields around it
    auto makeFoldShortcut = [this](const QKeySequence& sequence, auto slot, const QString& description) {
        auto* shortcut = new QShortcut(sequence, this);
        shortcut->setContext(Qt::WidgetShortcut);
        shortcut->setObjectName(description);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    makeFoldShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), &CodeEditor::foldAtCursor, "Fold the section / the block of code at the cursor");
    makeFoldShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), &CodeEditor::unfoldAtCursor, "Unfold the section / the block of code at the cursor");
    makeFoldShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Left), &CodeEditor::foldAll, "Fold everything");
    makeFoldShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_Right), &CodeEditor::unfoldAll, "Unfold everything");

    updateFolding();
}

void CodeEditor::scheduleFoldingUpdate()
{
    if (foldingTimer_)
    {
        foldingTimer_->start();
    }
}

bool CodeEditor::foldingAvailable() const
{
    return syntaxHighlighter != nullptr && syntaxMode() == SyntaxMode::Stc;
}

int CodeEditor::foldColumnWidth() const
{
    return foldingAvailable() ? std::max(12, fontMetrics().height() * 4 / 5) : 0;
}

int CodeEditor::lineNumbersRightEdge() const
{
    return lineNumberArea->width() - foldColumnWidth();
}

void CodeEditor::updateFolding()
{
    if (foldingTimer_)
    {
        foldingTimer_->stop();
    }

    foldRegions_.clear();
    if (foldingAvailable())
    {
        QStringList lines;
        lines.reserve(document()->blockCount());
        for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next())
        {
            lines.push_back(block.text());
        }
        foldRegions_ = stcfold::findRegions(lines);
    }

    foldRegionByStartLine_.clear();
    for (int index = 0; index < foldRegions_.size(); ++index)
    {
        // when a header and a block of code start in one line, the header (which is first) is the one with the marker
        if (!foldRegionByStartLine_.contains(foldRegions_[index].startLine))
        {
            foldRegionByStartLine_.insert(foldRegions_[index].startLine, index);
        }
    }

    // A folded region is still folded if the same kind of region starts in the line its anchor is in now
    QVector<FoldedRegion> stillFolded;
    QSet<int> foldedStartLines;
    for (const FoldedRegion& folded : std::as_const(foldedRegions_))
    {
        const int line = folded.anchor.blockNumber();
        const int index = foldRegionByStartLine_.value(line, -1);
        if (index < 0 || foldedStartLines.contains(line))
        {
            continue;
        }
        const stcfold::Region& region = foldRegions_[index];
        if (region.kind != folded.kind || region.level != folded.level)
        {
            continue;
        }
        stillFolded.push_back(folded);
        foldedStartLines.insert(line);
    }
    foldedRegions_ = stillFolded;

    // The text is edited at the cursor: that place cannot be hidden (typing Enter at the end of a folded header, an
    // undo, a search result, "go to line"... all end up here and unfold what covers the cursor)
    const int cursorLine = textCursor().blockNumber();
    const bool cursorWasHidden = !textCursor().block().isVisible();
    dropFoldsHidingLine(cursorLine);

    applyFolding();

    const int columnWidth = foldColumnWidth();
    if (columnWidth != lastFoldColumnWidth_)
    {
        lastFoldColumnWidth_ = columnWidth;
        updateLineNumberAreaWidth(0);
    }
    if (cursorWasHidden)
    {
        ensureCursorVisible();
    }
}

void CodeEditor::dropFoldsHidingLine(int line)
{
    foldedRegions_.removeIf([&](const FoldedRegion& folded) {
        const int index = foldRegionByStartLine_.value(folded.anchor.blockNumber(), -1);
        return index >= 0 && foldRegions_[index].hides(line);
    });
}

void CodeEditor::applyFolding()
{
    const int blocksCount = document()->blockCount();
    QVector<char> hidden(blocksCount, 0);
    for (const FoldedRegion& folded : std::as_const(foldedRegions_))
    {
        const int index = foldRegionByStartLine_.value(folded.anchor.blockNumber(), -1);
        if (index < 0)
        {
            continue;
        }
        const stcfold::Region& region = foldRegions_[index];
        for (int line = region.startLine + 1; line <= region.endLine && line < blocksCount; ++line)
        {
            hidden[line] = 1;
        }
    }

    foldedStartLines_.clear();
    for (const FoldedRegion& folded : std::as_const(foldedRegions_))
    {
        foldedStartLines_.insert(folded.anchor.blockNumber());
    }

    bool visibilityChanged = false;
    int line = 0;
    for (QTextBlock block = document()->firstBlock(); block.isValid(); block = block.next(), ++line)
    {
        const bool shouldBeVisible = !hidden[line];
        if (block.isVisible() == shouldBeVisible)
        {
            continue;
        }
        block.setVisible(shouldBeVisible);
        // QPlainTextEdit scrolls by lines and counts them from the blocks: a hidden block has none
        block.setLineCount(shouldBeVisible ? std::max(1, block.layout() ? block.layout()->lineCount() : 1) : 0);
        visibilityChanged = true;
    }

    if (visibilityChanged)
    {
        if (auto* layout = qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout()))
        {
            layout->requestUpdate();
            emit layout->documentSizeChanged(layout->documentSize()); // the range of the scroll bar
        }
    }
    viewport()->update();
    lineNumberArea->update();
}

bool CodeEditor::isFolded(int line) const
{
    return foldedStartLines_.contains(line);
}

int CodeEditor::innermostFoldableRegionAround(int line) const
{
    int best = -1;
    for (int index = 0; index < foldRegions_.size(); ++index)
    {
        const stcfold::Region& region = foldRegions_[index];
        if (!region.encloses(line) || isFolded(region.startLine))
        {
            continue;
        }
        if (best < 0 || region.startLine > foldRegions_[best].startLine
            || (region.startLine == foldRegions_[best].startLine && region.extentEndLine < foldRegions_[best].extentEndLine))
        {
            best = index;
        }
    }
    return best;
}

void CodeEditor::foldRegionsByIndex(const QVector<int>& indices)
{
    QVector<int> newlyFolded;
    for (const int index : indices)
    {
        const stcfold::Region& region = foldRegions_[index];
        if (isFolded(region.startLine))
        {
            continue;
        }
        QTextCursor anchor(document()->findBlockByNumber(region.startLine));
        anchor.setKeepPositionOnInsert(true); // text typed at the beginning of the line does not move the anchor to the next line
        foldedRegions_.push_back({anchor, region.kind, region.level});
        newlyFolded.push_back(index);
    }
    if (newlyFolded.isEmpty())
    {
        return;
    }

    // The cursor in a line which is going to be hidden goes to the end of the first line of the outermost region hiding it
    const int cursorLine = textCursor().blockNumber();
    int outermostStart = -1;
    for (const int index : std::as_const(newlyFolded))
    {
        const stcfold::Region& region = foldRegions_[index];
        if (region.hides(cursorLine) && (outermostStart < 0 || region.startLine < outermostStart))
        {
            outermostStart = region.startLine;
        }
    }
    if (outermostStart >= 0)
    {
        QTextCursor cursor(document()->findBlockByNumber(outermostStart));
        cursor.movePosition(QTextCursor::EndOfBlock);
        setTextCursor(cursor);
    }

    applyFolding();
}

void CodeEditor::foldLine(int line)
{
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    const int index = foldRegionByStartLine_.value(line, -1);
    if (index >= 0)
    {
        foldRegionsByIndex({index});
    }
}

void CodeEditor::unfoldLine(int line)
{
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    const auto removed = foldedRegions_.removeIf([line](const FoldedRegion& folded) {
        return folded.anchor.blockNumber() == line;
    });
    if (removed > 0)
    {
        applyFolding();
    }
}

void CodeEditor::toggleFoldAtLine(int line)
{
    if (isFolded(line))
    {
        unfoldLine(line);
    }
    else
    {
        foldLine(line);
    }
}

void CodeEditor::foldAtCursor()
{
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    const int index = innermostFoldableRegionAround(textCursor().blockNumber());
    if (index >= 0)
    {
        foldRegionsByIndex({index});
    }
}

void CodeEditor::unfoldAtCursor()
{
    unfoldLine(textCursor().blockNumber());
}

void CodeEditor::toggleFoldAtCursor()
{
    if (isFolded(textCursor().blockNumber()))
    {
        unfoldAtCursor();
    }
    else
    {
        foldAtCursor();
    }
}

void CodeEditor::foldAll()
{
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    QVector<int> all;
    for (int index = 0; index < foldRegions_.size(); ++index)
    {
        all.push_back(index);
    }
    foldRegionsByIndex(all);
}

void CodeEditor::foldAllCodeBlocks()
{
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    QVector<int> codeBlocksIndices;
    for (int index = 0; index < foldRegions_.size(); ++index)
    {
        if (foldRegions_[index].kind == stcfold::Kind::Code)
        {
            codeBlocksIndices.push_back(index);
        }
    }
    foldRegionsByIndex(codeBlocksIndices);
}

void CodeEditor::unfoldAll()
{
    if (foldedRegions_.isEmpty())
    {
        return;
    }
    foldedRegions_.clear();
    applyFolding();
}

// ------------------------------------------------------------------------------------------------ drawing

void CodeEditor::paintFoldMarker(QPainter& painter, int blockNumber, int top) const
{
    if (!foldRegionByStartLine_.contains(blockNumber))
    {
        return;
    }

    const bool folded = isFolded(blockNumber);
    const int rowHeight = fontMetrics().height();
    const QPointF center(lineNumbersRightEdge() + foldColumnWidth() / 2.0, top + rowHeight / 2.0);
    const qreal half = rowHeight * 0.2;

    QPolygonF triangle;
    if (folded) // points to the right: "there is more"
    {
        triangle << QPointF(center.x() - half * 0.8, center.y() - half * 1.2)
                 << QPointF(center.x() - half * 0.8, center.y() + half * 1.2)
                 << QPointF(center.x() + half * 1.0, center.y());
    }
    else // points down: "this is open"
    {
        triangle << QPointF(center.x() - half * 1.2, center.y() - half * 0.8)
                 << QPointF(center.x() + half * 1.2, center.y() - half * 0.8)
                 << QPointF(center.x(), center.y() + half * 1.0);
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(folded ? QColor("#202020") : QColor("#707070"));
    painter.drawPolygon(triangle);
    painter.restore();
}

void CodeEditor::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    // The positions are worked out on every paint (not only the part which is repainted), the clicks use them
    foldLabels_.clear();
    if (foldedRegions_.isEmpty())
    {
        return;
    }

    QPainter painter(viewport());
    painter.setClipRect(event->rect());
    painter.setRenderHint(QPainter::Antialiasing);

    QFont labelFont = font();
    if (font().pointSizeF() > 0)
    {
        labelFont.setPointSizeF(font().pointSizeF() * 0.85);
    }
    painter.setFont(labelFont);
    const QFontMetrics labelMetrics(labelFont);

    QColor ink = palette().color(QPalette::Text);
    QColor border = ink;
    border.setAlpha(110);
    QColor fill = ink;
    fill.setAlpha(28);
    ink.setAlpha(190);

    for (const FoldedRegion& folded : std::as_const(foldedRegions_))
    {
        const int line = folded.anchor.blockNumber();
        const int index = foldRegionByStartLine_.value(line, -1);
        const QTextBlock block = document()->findBlockByNumber(line);
        if (index < 0 || !block.isValid() || !block.isVisible()) // the first line of a region inside of a folded one is not seen
        {
            continue;
        }

        QTextCursor endOfLine(block);
        endOfLine.movePosition(QTextCursor::EndOfBlock);
        const QRect caret = cursorRect(endOfLine); // the end of the last row of the line when it is wrapped
        if (caret.bottom() < 0 || caret.top() > viewport()->height())
        {
            continue;
        }

        const QString text = QStringLiteral("\u2026 ") + tr("%n line(s)", "label at the end of a folded line", foldRegions_[index].hiddenLinesCount());
        const QRect label(caret.right() + 8, caret.top() + 1, labelMetrics.horizontalAdvance(text) + 12, std::max(4, caret.height() - 2));
        foldLabels_.push_back({label, line});

        painter.setPen(QPen(border, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(label).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        painter.setPen(ink);
        painter.drawText(label, Qt::AlignCenter, text);
    }
}

int CodeEditor::foldLabelLineAt(const QPoint& viewportPoint) const
{
    for (const auto& [rect, line] : foldLabels_)
    {
        if (rect.contains(viewportPoint))
        {
            return line;
        }
    }
    return -1;
}

void CodeEditor::addFoldingActions(QMenu* menu)
{
    if (!foldingAvailable())
    {
        return;
    }
    if (foldingTimer_ && foldingTimer_->isActive())
    {
        updateFolding();
    }
    if (foldRegions_.isEmpty())
    {
        return;
    }

    const int cursorLine = textCursor().blockNumber();
    QMenu* submenu = menu->addMenu(tr("Folding"));
    if (isFolded(cursorLine))
    {
        submenu->addAction(tr("Unfold"), this, &CodeEditor::unfoldAtCursor);
    }
    else if (innermostFoldableRegionAround(cursorLine) >= 0)
    {
        submenu->addAction(tr("Fold this section or block of code"), this, &CodeEditor::foldAtCursor);
    }
    submenu->addSeparator();
    submenu->addAction(tr("Fold everything"), this, &CodeEditor::foldAll);
    submenu->addAction(tr("Fold all blocks of code"), this, &CodeEditor::foldAllCodeBlocks);
    QAction* unfoldEverything = submenu->addAction(tr("Unfold everything"), this, &CodeEditor::unfoldAll);
    unfoldEverything->setEnabled(!foldedRegions_.isEmpty());
}
