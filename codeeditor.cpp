/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#include <QFile>
#include <QPainter>
#include <QTextBlock>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>
#include <QTimer>
#include <QMimeData>
#include <QFileInfo>
#include <QImageReader>
#include "codeeditor.h"
#include "linenumberarea.h"
#include "stcsyntaxhighlighter.h"


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
} // namespace

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
    setAcceptDrops(true);
    setMouseTracking(true);

    lineNumberArea = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::totalLinesCountChanged);
    // connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CodeEditor::onScrollChanged);
    connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &CodeEditor::fileChanged);

    updateLineNumberAreaWidth(0);
    // highlightCurrentLine();

    STCSyntaxHighlighter *highlighter = new STCSyntaxHighlighter(document());
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

    setFileName(fileName);

    return true;
}

void CodeEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu* menu = createStandardContextMenu();

    QTextCursor selection = textCursor();

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
    }

    menu->exec(event->globalPos());
    delete menu;
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
    {
        switch(event->key())
        {
        case Qt::Key_B:
            emit shortcutPressed_bold();
            return;
        case Qt::Key_U:
            emit shortcutPressed_run();
            return;
        case Qt::Key_W:
            emit shortcutPressed_warning();
            return;
        case Qt::Key_T:
            emit shortcutPressed_tip();
            return;
        case Qt::Key_H:
            emit shortcutPressed_href();
            return;
        case Qt::Key_1:
            emit shortcutPressed_h1();
            return;
        case Qt::Key_2:
            emit shortcutPressed_h2();
            return;
        case Qt::Key_3:
            emit shortcutPressed_h3();
            return;
        case Qt::Key_4:
            emit shortcutPressed_h4();
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
