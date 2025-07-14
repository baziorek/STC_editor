/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#include <QPainter>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>
#include <QTimer>
#include <QMimeData>
#include <QFileInfo>
#include <QImageReader>
#include <QShortcut>
#include <QProcess>
#include "codeeditor.h"
#include "linenumberarea.h"
#include "stcsyntaxhighlighter.h"
#include "ui/cppcompilerdialog.h"
#include "utils/diffcalculation.h"
#include "types/CodeBlock.h"


namespace
{
constexpr int spacesPerTab = 4;

/// in Qt it is impossible to check if we really have text file (without external library).
/// That is why we are checking first characters and checking if they are printable:
bool isProbablyTextFile(const QString &filePath, int maxBytesToCheck = 2048)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QByteArray buffer = file.read(maxBytesToCheck);
    file.close();

    for (char byte : buffer)
    {
        uchar c = static_cast<uchar>(byte);

        // Printable ASCII + common whitespace (tab, newline, carriage return)
        if ((c >= 0x20 && c <= 0x7E) || c == '\n' || c == '\r' || c == '\t')
            continue;

        // UTF-8 continuation bytes or BOM may appear — allow some
        if (c >= 0x80)
            continue;

        // Otherwise, probably binary
        return false;
    }

    return true;
}

bool operator==(const CodeBlock& a, const CodeBlock& b)
{
    return a.cursor.selectionStart() == b.cursor.selectionStart()
        && a.cursor.selectionEnd() == b.cursor.selectionEnd()
        && a.tag == b.tag
        && a.language == b.language;
}
} // namespace

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    setAcceptDrops(true);
    setMouseTracking(true);

    registerShortcuts();

    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::totalLinesCountChanged);
    // connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CodeEditor::onScrollChanged);
    connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &CodeEditor::fileChanged);

    connect(this, &CodeEditor::textChanged, this, [this]() {
        this->lastChangeTime = QDateTime::currentDateTime();
        updateDiffWithOriginal();
    });
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::onCursorPositionChanged);
    connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::onContentsChange);


    updateLineNumberAreaWidth(0);
    // highlightCurrentLine();

    STCSyntaxHighlighter *highlighter = new STCSyntaxHighlighter(document());
}
CodeEditor::~CodeEditor() = default;

void CodeEditor::registerShortcuts()
{
    auto makeShortcut = [this](const QKeySequence& seq, const auto& signal, const QString& description) {
        QShortcut* sc = new QShortcut(seq, this);
        sc->setObjectName(description);
        connect(sc, &QShortcut::activated, this, signal);
    };

    // makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), &CodeEditor::shortcutPressed_bold,    "Bold selected text"); // nie dziala
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_U), &CodeEditor::shortcutPressed_run,     "Insert [run] tags");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), &CodeEditor::shortcutPressed_warning, "Insert [div class=\"uwaga\"] block");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), &CodeEditor::shortcutPressed_tip,     "Insert [div class=\"tip\"] block");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), &CodeEditor::shortcutPressed_href,    "Insert hyperlink");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_1), &CodeEditor::shortcutPressed_h1,      "Header level 1");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_2), &CodeEditor::shortcutPressed_h2,      "Header level 2");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_3), &CodeEditor::shortcutPressed_h3,      "Header level 3");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_4), &CodeEditor::shortcutPressed_h4,      "Header level 4");

    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), &CodeEditor::increaseFontSize,    "Increase font size"); // Ctrl +
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus),  &CodeEditor::increaseFontSize,    "Increase font size (PLUS key)");
    makeShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), &CodeEditor::decreaseFontSize,    "Decrease font size");
}


int CodeEditor::lineNumberAreaWidth()
{
    const int maxLine = linesCount();
    const int digits4LineNumber = std::log10(maxLine) + 1;

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits4LineNumber;
    return space;
}

bool CodeEditor::noUnsavedChanges() const
{
    const auto currentlyVisibleText = toPlainText();
    if (openedFileName.isEmpty())
    {
        return currentlyVisibleText.isEmpty();
    }

    QFile file(openedFileName);
    if (!file.exists())
    {
        return false;
    }
    //  TODO: consider: !file.exists() && currentlyVisibleText.isEmpty()

    file.open(QFile::ReadOnly);
    return QString::fromUtf8(file.readAll()) == currentlyVisibleText;
}

void CodeEditor::setFileName(const QString &newFileName)
{
    openedFileName = newFileName;
    if (! openedFileName.isEmpty() && QFile::exists(openedFileName))
    {
        enableWatchingOfFile(openedFileName);
    }
    else
    {
        fileWatcher.removePaths(fileWatcher.files());
    }
}

void CodeEditor::enableWatchingOfFile(const QString &newFileName)
{
    if (fileWatcher.files().contains(newFileName))
        fileWatcher.removePath(newFileName);

    fileWatcher.addPath(newFileName);
}

void CodeEditor::go2LineRequested(int lineNumber)
{
    lineNumber = std::clamp(lineNumber, 1, linesCount());

    QTextCursor cursor = cursor4Line(lineNumber);
    setTextCursor(cursor);
    ensureCursorVisible();

    setFocus();
}

void CodeEditor::goToLineAndOffset(int lineNumber, int linePosition)
{
    lineNumber = std::clamp(lineNumber, 1, linesCount());

    QTextCursor cursor = cursor4Line(lineNumber);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, linePosition);
    setTextCursor(cursor);
    ensureCursorVisible();

    setFocus();
}

void CodeEditor::onScrollChanged(int)
{
    const int total = blockCount();
    const int firstVisible = cursorForPosition(QPoint(0, 0)).block().blockNumber() + 1;
    const int lastVisible = cursorForPosition(QPoint(0, height() - 1)).block().blockNumber() + 1;

    const int visibleLines = lastVisible - firstVisible + 1;
    const int percentage = std::clamp((100 * lastVisible) / std::max(1, total), 0, 100);

    QString info = QString("Lines %1–%2 of %3 (%4%)")
                       .arg(firstVisible)
                       .arg(lastVisible)
                       .arg(total)
                       .arg(percentage);

    QToolTip::showText(mapToGlobal(QPoint(width() - 100, height() / 2)), info, this);
}

void CodeEditor::onCursorPositionChanged()
{
    const int newLine = textCursor().blockNumber();
    if (newLine != currentLine)
    {
        currentLine = newLine;
        lineNumberArea->update(); // wymusza przerysowanie obszaru z numerami linii
    }
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::reloadFromFile(bool discardChanges)
{
    const auto currentLineBeforeReloading = textCursor().block().blockNumber();
    const auto currentColumnBeforeReloading = textCursor().positionInBlock();

    if (loadFileContentDistargingCurrentContent(openedFileName))
    {
        // restore cursor position
        QTextCursor cursor = cursor4Line(currentLineBeforeReloading + 1); // +1 because lines are being counted from 1
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, currentColumnBeforeReloading);
        setTextCursor(cursor);
        ensureCursorVisible();
    }
}

bool CodeEditor::loadFileContentDistargingCurrentContent(const QString& fileName)
{
    QFile file(fileName);
    if (!file.exists())
    {
        QMessageBox::warning(this, tr("Reloading error"), tr("File '%1' does not exist!").arg(openedFileName));
        return false;
    }
    if (!file.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, tr("Reloading error"), tr("Could not open '%1'.").arg(openedFileName));
        return false;
    }

    const QByteArray content = file.readAll();
    file.close();

    setPlainText(QString::fromUtf8(content));

    trackOriginalVersionOfFile(fileName);

    setFileName(fileName);

    analizeEntireDocumentDetectingCodeBlocks();

    return true;
}
void CodeEditor::trackOriginalVersionOfFile(const QString& fileName)
{
    originalLines = toPlainText().split('\n');
    modifiedLines.clear();
    fileModificationTime = QFileInfo(fileName).lastModified();
    lastChangeTime = QDateTime(); // reset
}

QMultiMap<QString, QKeySequence> CodeEditor::listOfShortcuts() const
{
    QMultiMap<QString, QKeySequence> result;

    for (QShortcut* s : findChildren<QShortcut*>()) {
        QString desc = s->objectName();
        if (desc.isEmpty())
            desc = "Editor shortcut";
        result.insert(desc, s->key());
    }

    return result;
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    // Move cursor to click position
    QTextCursor clickCursor = cursorForPosition(event->pos());

    // remove selection if clicked out of the selection
    QTextCursor current = textCursor();
    if (current.hasSelection())
    {
        int clickPos = clickCursor.position();
        if (clickPos < current.selectionStart() || clickPos > current.selectionEnd())
        {
            setTextCursor(clickCursor);  // move current text cursor
        }
    }
    else
    {
        setTextCursor(clickCursor); // move current text cursor
    }

    QTextCursor selection = textCursor();

    QMenu* menu = createStandardContextMenu();

    if (selection.hasSelection())
    {
        QString selectedText = selection.selectedText();
        bool isSingleWord = !selectedText.contains(QRegularExpression("\\s"));

        menu->addSeparator();

        // Uppercase
        if (!selectedText.isUpper())
        {
            QAction* actionUpper = new QAction("To UPPER CASE", this);
            connect(actionUpper, &QAction::triggered, this, [this]() {
                QTextCursor sel = textCursor();
                sel.insertText(sel.selectedText().toUpper());
            });
            menu->addAction(actionUpper);
        }

        // Lowercase
        if (!selectedText.isLower())
        {
            QAction* actionLower = new QAction("To lower case", this);
            connect(actionLower, &QAction::triggered, this, [this]() {
                QTextCursor sel = textCursor();
                sel.insertText(sel.selectedText().toLower());
            });
            menu->addAction(actionLower);
        }

        // CamelCase <-> snake_case
        if (isSingleWord) {
            QString text = selectedText;

            QAction* casingAction = nullptr;
            if (text.contains('_')) {
                casingAction = new QAction("To camelCase", this);
                connect(casingAction, &QAction::triggered, this, [this, text]() {
                    QStringList parts = text.split('_', Qt::SkipEmptyParts);
                    for (int i = 1; i < parts.size(); ++i)
                        parts[i][0] = parts[i][0].toUpper();
                    QString camel = parts.join("");
                    QTextCursor sel = textCursor();
                    sel.insertText(camel);
                });
            } else {
                casingAction = new QAction("To snake_case", this);
                connect(casingAction, &QAction::triggered, this, [this, text]() {
                    QString snake;
                    for (int i = 0; i < text.length(); ++i) {
                        if (i > 0 && text[i].isUpper()) {
                            snake += '_';
                            snake += text[i].toLower();
                        } else {
                            snake += text[i].toLower();
                        }
                    }
                    QTextCursor sel = textCursor();
                    sel.insertText(snake);
                });
            }

            if (casingAction)
                menu->addAction(casingAction);
        }

        // Adding numeration - if selected more than one lines:
        int start = selection.selectionStart();
        int end = selection.selectionEnd();
        int startLine = document()->findBlock(start).blockNumber();
        int endLine = document()->findBlock(end).blockNumber();

        if (endLine > startLine)
        {
            menu->addSeparator();

            // Numeration 1. 2. 3.
            QAction* numberedList = new QAction("Add numeration: 1., 2., 3. ...", this);
            connect(numberedList, &QAction::triggered, this, [this, startLine, endLine]() {
                QTextCursor cursor(document()->findBlockByNumber(startLine));
                for (int i = startLine, n = 1; i <= endLine; ++i, ++n)
                {
                    QTextBlock block = document()->findBlockByNumber(i);
                    if (block.isValid()) {
                        QTextCursor lineCursor(block);
                        lineCursor.movePosition(QTextCursor::StartOfBlock);
                        lineCursor.insertText(QString::number(n) + ". ");
                    }
                }
            });
            menu->addAction(numberedList);

            // Numeration: -
            QAction* dashedList = new QAction("Add numeration: bullet points", this);
            connect(dashedList, &QAction::triggered, this, [this, startLine, endLine]() {
                for (int i = startLine; i <= endLine; ++i)
                {
                    QTextBlock block = document()->findBlockByNumber(i);
                    if (block.isValid()) {
                        QTextCursor lineCursor(block);
                        lineCursor.movePosition(QTextCursor::StartOfBlock);
                        lineCursor.insertText("- ");
                    }
                }
            });
            menu->addAction(dashedList);

            // Join lines with space
            QAction* joinLines = new QAction("Join lines with space", this);
            connect(joinLines, &QAction::triggered, this, [this]() {
                QTextCursor sel = textCursor();
                QString joined = sel.selectedText();
                joined.replace(QChar::ParagraphSeparator, " ");
                sel.insertText(joined);
            });
            menu->addAction(joinLines);
        }
    }
    else // if (! selection.hasSelection())
    {
        // No selection - we are cheching if cursor is inside one of tags
        QTextCursor cursor = textCursor();
        QString blockText = cursor.block().text();
        int posInBlock = cursor.position() - cursor.block().position();

        static const QStringList tagList =
        {
            "code", "cpp", "py", "b", "u", "i", "h1", "h2", "h3", "h4", "run"
        };

        for (const QString& tag : tagList)
        {
            QRegularExpression regex(QString(R"(\[%1\](.*?)\[/%1\])").arg(tag));
            QRegularExpressionMatchIterator it = regex.globalMatch(blockText);

            while (it.hasNext())
            {
                QRegularExpressionMatch match = it.next();
                int start = match.capturedStart(1);
                int end = match.capturedEnd(1);

                if (posInBlock >= start && posInBlock <= end)
                {
                    // cursor inside one of tags
                    menu->addSeparator();
                    QAction* removeTag = new QAction(QString("Remove [%1]").arg(tag), this);
                    connect(removeTag, &QAction::triggered, this, [this, tag]() {
                        QTextCursor cursor = textCursor();
                        QString blockText = cursor.block().text();

                        // Find position of cursor in line
                        int posInBlock = cursor.position() - cursor.block().position();

                        QRegularExpression regex(QString(R"(\[%1\](.*?)\[/%1\])").arg(tag));
                        QRegularExpressionMatchIterator it = regex.globalMatch(blockText);

                        while (it.hasNext())
                        {
                            QRegularExpressionMatch match = it.next();
                            int start = match.capturedStart(1);
                            int end = match.capturedEnd(1);

                            if (posInBlock >= start && posInBlock <= end) {
                                QTextCursor lineCursor(cursor.block());
                                lineCursor.setPosition(cursor.block().position() + match.capturedStart());
                                lineCursor.setPosition(cursor.block().position() + match.capturedEnd(), QTextCursor::KeepAnchor);

                                QString inner = match.captured(1);
                                lineCursor.insertText(inner);
                                break;
                            }
                        }
                    });
                    menu->addAction(removeTag);
                    break; // only first matching tag
                }
            }
        }

        const int clickedPos = cursorForPosition(event->pos()).position();
        if (auto maybeBlock = selectEnclosingCodeBlock(clickedPos); maybeBlock.has_value())
        {
            QTextCursor cursor = maybeBlock->cursor;
            QString tag = maybeBlock->tag;

            menu->addSeparator();

            QAction* selectAllCodeAction = new QAction("Select this source code", this);
            connect(selectAllCodeAction, &QAction::triggered, this, [this, cursor]() {
                setTextCursor(cursor);
            });
            menu->addAction(selectAllCodeAction);

            if ("cpp" == tag)
            {
                QAction* formatCppAction = new QAction("Format C++ with clang-format", this);
                connect(formatCppAction, &QAction::triggered, this, [this, cursor]() mutable {
                    QString rawCode = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
                    QString formatted = formatCppWithClang(rawCode);

                    if (!formatted.isEmpty())
                    {
                        cursor.insertText(formatted.replace(QChar::LineSeparator, "\n"));
                    }
                    else
                    {
                        QMessageBox::warning(this, "clang-format", "Formatting failed or clang-format not available.");
                    }
                });
                menu->addAction(formatCppAction);

                QAction* compileCppAction = new QAction("Compile C++ with g++", this);
                connect(compileCppAction, &QAction::triggered, this, [this, cursor]() {
                    QString rawCode = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
                    auto* dialog = new CppCompilerDialog(rawCode, this);
                    dialog->exec();
                });
                menu->addAction(compileCppAction);
            }
        }
    }

    menu->exec(event->globalPos());
    delete menu;
}

QString CodeEditor::formatCppWithClang(const QString& code)
{
    QProcess clang;
    clang.start("clang-format", QStringList() << "-style=LLVM");

    if (!clang.waitForStarted(1000))
        return {};

    clang.write(code.toUtf8());
    clang.closeWriteChannel();

    if (!clang.waitForFinished(1000))
        return {};

    return QString::fromUtf8(clang.readAllStandardOutput()).trimmed();
}

void CodeEditor::restoreStateWhichDoesNotRequireSaving(bool discardChanges)
{
    if (openedFileName.isEmpty())
    {
        clear();
    }
    else
    {
        reloadFromFile(discardChanges);
    }
}

QTextCursor CodeEditor::cursor4Line(int lineNumber) const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::Start);
    for (int i = 1; i < lineNumber; ++i)
    {
        cursor.movePosition(QTextCursor::NextBlock);
    }
    return cursor;
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            if (modifiedLines.contains(blockNumber + 1))
            {
                painter.fillRect(0, top, lineNumberArea->width(), fontMetrics().height(), QColor("#FFDD88"));
            }

            // Drawing arrow in the current position
            if (blockNumber == currentLine)
            {
                const int yCenter = top + fontMetrics().height()/2;
                const int arrowHeight = 6;

                painter.setPen(Qt::NoPen);
                painter.setBrush(Qt::yellow);

                QPolygon arrow;
                arrow << QPoint(lineNumberArea->width() - 1, yCenter) // top of arrow on the right edge
                     << QPoint(0, yCenter - arrowHeight/2)  // upper left corner
                     << QPoint(0, yCenter + arrowHeight/2); // lower button corner

                painter.drawPolygon(arrow);
            }

            // Text on arrow
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);
            painter.drawText(0, top, lineNumberArea->width(), fontMetrics().height(),
                           Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier) && !(event->modifiers() & ~Qt::ControlModifier))
    { // TODO: This shortcut should be set up in constructor, but it is not working
        if (Qt::Key_B == event->key())
        {
            emit shortcutPressed_bold();
            return;
        }
    }

    if (event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier))
    {
        QTextCursor cursor = textCursor();

        if (cursor.hasSelection())
        {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            cursor.setPosition(start);
            int firstBlock = cursor.blockNumber();

            cursor.setPosition(end);
            if (cursor.position() > 0 && cursor.atBlockStart())
                cursor.movePosition(QTextCursor::PreviousBlock);
            int lastBlock = cursor.blockNumber();

            cursor.beginEditBlock();
            for (int i = firstBlock; i <= lastBlock; ++i)
            {
                QTextBlock block = document()->findBlockByNumber(i);
                QTextCursor blockCursor(block);
                blockCursor.movePosition(QTextCursor::StartOfBlock);
                blockCursor.insertText(QString(spacesPerTab, ' '));
            }
            cursor.endEditBlock();
        }
        else
        {
            insertPlainText(QString(spacesPerTab, ' '));
        }
        return;
    }

    if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && (event->modifiers() & Qt::ShiftModifier))) {
        QTextCursor cursor = textCursor();

        if (cursor.hasSelection())
        {
            int start = cursor.selectionStart();
            int end = cursor.selectionEnd();

            cursor.setPosition(start);
            int firstBlock = cursor.blockNumber();

            cursor.setPosition(end);
            if (cursor.position() > 0 && cursor.atBlockStart())
                cursor.movePosition(QTextCursor::PreviousBlock);
            int lastBlock = cursor.blockNumber();

            cursor.beginEditBlock();
            for (int i = firstBlock; i <= lastBlock; ++i)
            {
                QTextBlock block = document()->findBlockByNumber(i);
                QTextCursor blockCursor(block);
                blockCursor.movePosition(QTextCursor::StartOfBlock);

                blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 1);
                QString firstChar = blockCursor.selectedText();

                if (firstChar == "\t")
                {
                    blockCursor.removeSelectedText();
                }
                else
                {
                    // try to remove 4 spaces in the beginning
                    blockCursor = QTextCursor(block);
                    for (int j = 0; j < spacesPerTab; ++j)
                    {
                        blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                        if (blockCursor.selectedText().endsWith(" "))
                        {
                            blockCursor.removeSelectedText();
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            cursor.endEditBlock();
        }
        return;
    }

    // default behaviour:
    QPlainTextEdit::keyPressEvent(event);
}


void CodeEditor::fileChanged(const QString &path)
{
    // stop watching file to avoid multiple signals
    fileWatcher.removePath(path);

    // delayed reaction
    QTimer::singleShot(300, this, [this, path]() {
        QFile file(path);

        if (!file.exists())
        {
            QMessageBox::warning(this, tr("File removed"),
                                 tr("File '%1' has been removed from disk.").arg(path));
            return; // we stop listening because file does not exist anymore
        }

        if (!file.open(QFile::ReadOnly))
        {
            QMessageBox::warning(this, tr("File error"),
                                 tr("Cannot open file '%1'.").arg(path));
            return;
        }

        const QByteArray newContent = file.readAll();
        file.close();

        const QByteArray currentContent = toPlainText().toUtf8();
        if (newContent == currentContent)
        {
            enableWatchingOfFile(path);
            return;
        }

        QMessageBox::StandardButton response = QMessageBox::question(
            this,
            tr("File changed"),
            tr("File '%1' has been modified outside of the editor.\n\n"
               "Do you want to reload it?")
                .arg(path),
            QMessageBox::Yes | QMessageBox::No);

        if (response == QMessageBox::Yes)
        {
            reloadFromFile(true);
        }

        enableWatchingOfFile(path);
    });
}

void CodeEditor::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QPlainTextEdit::dragEnterEvent(event);
}

void CodeEditor::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (! mimeData->hasUrls())
    {
        return;
    }

    QList<QUrl> urls = mimeData->urls();
    for (const QUrl &url : urls)
    {
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty())
        {
            continue;
        }

        QFileInfo fileInfo(localPath);
        const QString suffix = fileInfo.suffix().toLower();

        // 1. Handle text files
        if (isProbablyTextFile(localPath))
        {
            if (loadFileContentDistargingCurrentContent(localPath))
            {
                break; // only one file at once
            }
        }
        // 2. Add link to image file
        else if (QImageReader::supportedImageFormats().contains(suffix.toUtf8()))
        {
            QTextCursor cursor = textCursor();
            cursor.insertText(QString(R"([img src="%1"])").arg(localPath));
        }
    }

    event->acceptProposedAction();
}

void CodeEditor::mouseMoveEvent(QMouseEvent* event)
{
    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);
    const QString word = cursor.selectedText();

    static QRegularExpression imgRegex("\\[img\\s+src=\"([^\"]+)\"\\]");
    QTextBlock block = cursor.block();
    const QString blockText = block.text();
    QRegularExpressionMatch match = imgRegex.match(blockText);
    if (match.hasMatch()) {
        QString imagePath = match.captured(1);
        QFileInfo fi(imagePath);
        if (fi.exists() && fi.isFile() &&
            QImageReader::supportedImageFormats().contains(fi.suffix().toLower().toUtf8()))
        {
            QImage image(imagePath);
            if (!image.isNull()) {
                const QSize previewSize = image.size().boundedTo(QSize(200, 150));
                const QPixmap pixmap = QPixmap::fromImage(image.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

                // To make sure tooltip is not hiding instantly after moving mouse
                if (imagePath != lastTooltipImagePath)
                {
                    lastTooltipImagePath = imagePath;

                    const QString tooltipHtml = QString(R"(
                        <b>%1</b><br/>
                        <img src="%2" height="%3"/><br/>
                        <i>%4 x %5 px</i><br/>
                        Last modified: %6
                    )")
                                                    .arg(fi.fileName())
                                                    .arg(imagePath)
                                                    // .arg(previewSize.width())
                                                    .arg(previewSize.height())
                                                    .arg(image.width())
                                                    .arg(image.height())
                                                    .arg(fi.lastModified().toString(Qt::ISODate));

                    QToolTip::showText(event->globalPosition().toPoint(), tooltipHtml, this);
                }

                return;
            }
        }
    }

    // If regex does not match: hide tool tip and reset path
    lastTooltipImagePath.clear();
    QToolTip::hideText();
    QPlainTextEdit::mouseMoveEvent(event);
}

void CodeEditor::increaseFontSize()
{
    QFont f = font();
    f.setPointSize(std::min(f.pointSize() + 1, 72));
    setFont(f);
}

void CodeEditor::decreaseFontSize()
{
    QFont f = font();
    f.setPointSize(std::max(f.pointSize() - 1, 4));
    setFont(f);
}

std::optional<CodeBlock> CodeEditor::selectEnclosingCodeBlock(int cursorPos)
{
    if (auto codeInfo = getCodeTagAtPosition(cursorPos))
    {
        for (const auto& block : codeBlocks)
        {
            if (block.tag == codeInfo->tag && block.cursor.selectionStart() == codeInfo->position)
            {
                CodeBlock codeOnlyBlock = block;

                QString blockText = block.cursor.selectedText();

                // Detecting code by removing STC tags:
                QRegularExpression tagPattern(QString("^\\[%1\\](.*)\\[\\/%1\\]$")
                    .arg(QRegularExpression::escape(block.tag)));

                auto match = tagPattern.match(blockText);
                if (match.hasMatch())
                {
                    QTextCursor codeCursor = block.cursor;
                    codeCursor.setPosition(block.cursor.selectionStart() + match.capturedStart(1));
                    codeCursor.setPosition(block.cursor.selectionStart() + match.capturedEnd(1), QTextCursor::KeepAnchor);

                    codeOnlyBlock.cursor = codeCursor;
                    return codeOnlyBlock;
                }
            }
        }
    }

    return std::nullopt;
}

void CodeEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        const int numDegrees = event->angleDelta().y();
        if (numDegrees > 0)
        {
            increaseFontSize();
        }
        else if (numDegrees < 0)
        {
            decreaseFontSize();
        }

        event->accept(); // stop propagation
        return;
    }

    // default handling
    QPlainTextEdit::wheelEvent(event);
}

void CodeEditor::updateDiffWithOriginal()
{
    const QStringList currentLines = toPlainText().split('\n');
    const QSet<int> newDiff = calculateModifiedLines(originalLines, currentLines);

    if (newDiff != modifiedLines)
    {
        modifiedLines = newDiff;
        emit numberOfModifiedLinesChanged(modifiedLines.size());
    }

    // Optional: emit signal to refresh UI with updated status bar
    emit totalLinesCountChanged(linesCount());

    // repaint margin
    lineNumberArea->update();
}

void CodeEditor::markAsSaved()
{
    originalLines = toPlainText().split('\n');
    modifiedLines.clear();
    lastChangeTime = {};
    fileModificationTime = QFileInfo(openedFileName).lastModified();
    lineNumberArea->update();

    emit numberOfModifiedLinesChanged(0);
}

QString CodeEditor::getFileModificationInfoText() const
{
    QString fileDate = fileModificationTime.toString("yyyy-MM-dd hh:mm:ss");
    QString editTime;

    if (lastChangeTime.isValid())
    {
        if (fileModificationTime.date() == lastChangeTime.date())
        {
            editTime = lastChangeTime.toString("hh:mm:ss");
        }
        else
        {
            editTime = lastChangeTime.toString("yyyy-MM-dd hh:mm:ss");
        }
    }

    if (!editTime.isEmpty())
    {
        return QString("Changed lines: %1 (time of unsaved changes: %2, time of file modification: %3)")
            .arg(modifiedLines.size())
            .arg(editTime)
            .arg(fileDate);
    }
    else
    {
        return QString("File: %1 (no changes)").arg(fileDate);
    }
}

bool CodeEditor::isInsideCode(int position) const
{
    return static_cast<bool>(getCodeTagAtPosition(position));
}
std::optional<CodeEditor::CodeBlockInfo> CodeEditor::getCodeTagAtPosition(int position) const
{
    for (const auto& block : codeBlocks)
    {
        if (position >= block.cursor.selectionStart() && position <= block.cursor.selectionEnd())
        {
            return CodeBlockInfo{block.tag, block.cursor.selectionStart()};
        }
    }
    return std::nullopt;
}

void CodeEditor::analizeEntireDocumentDetectingCodeBlocks()
{
    codeBlocks = parseAllCodeBlocks();
    emit codeBlocksChanged();
}

void CodeEditor::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    QTextBlock block = document()->findBlock(position);
    if (!block.isValid())
    {
        analizeEntireDocumentDetectingCodeBlocks();
        return;
    }

    const QString line = block.text();

    static const QRegularExpression openTagRe(R"(\[(cpp|code|py|log)(\s+src\s*=\s*"[^"]*")?\])", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression closeTagRe(R"(\[/\s*(cpp|code|py|log)\s*\])", QRegularExpression::CaseInsensitiveOption);

    bool lineHasOpen = openTagRe.match(line).hasMatch();
    bool lineHasClose = closeTagRe.match(line).hasMatch();

    if (lineHasOpen || lineHasClose)
    {
        analizeEntireDocumentDetectingCodeBlocks();
    }
    else
    {
        if (isInsideCode(position))
        {
            emit codeBlocksChanged();
        }
    }
}

QVector<CodeBlock> CodeEditor::parseAllCodeBlocks()
{
    QVector<CodeBlock> result;
    QString text = toPlainText();

    QRegularExpression tagRe(R"__(\[(cpp|code|py|log)(\s+src\s*=\s*"([^"]*)")?\](.*?)\[/\s*\1\s*\])__",
                             QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

    QRegularExpressionMatchIterator it = tagRe.globalMatch(text);
    while (it.hasNext())
    {
        auto match = it.next();
        QTextCursor c = textCursor();
        c.setPosition(match.capturedStart(0));
        c.setPosition(match.capturedEnd(0), QTextCursor::KeepAnchor);

        CodeBlock b;
        b.cursor = c;
        b.tag = match.captured(1).toLower();
        b.language = match.captured(3).toLower();
        result.append(b);
    }
    return result;
}
