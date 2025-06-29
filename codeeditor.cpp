/// the code of the class is copied from: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#include <QFile>
#include <QPainter>
#include <QTextBlock>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>
#include <QTimer>
#include "codeeditor.h"
#include "linenumberarea.h"
#include "stcsyntaxhighlighter.h"


CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
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
    return file.readAll() == currentlyVisibleText;
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
    QFile file(openedFileName);
    if (!file.exists())
    {
        QMessageBox::warning(this, tr("Reloading error"), tr("File '%1' does not exist!").arg(openedFileName));
        return;
    }

    const auto currentLineBeforeReloading = textCursor().block().blockNumber();
    const auto currentColumnBeforeReloading = textCursor().positionInBlock();
    if (!file.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, tr("Reloading error"), tr("Could not open '%1'.").arg(openedFileName));
        return;
    }

    const QByteArray content = file.readAll();
    file.close();

    // load content
    setPlainText(QString::fromUtf8(content));

    // restore cursor position
    QTextCursor cursor = cursor4Line(currentLineBeforeReloading + 1); // +1 because lines are being counted from 1
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, currentColumnBeforeReloading);
    setTextCursor(cursor);
    ensureCursorVisible();
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
    }

    // // Stała opcja
    // menu->addSeparator();
    // QAction* customAction = new QAction("Moja opcja", this);
    // connect(customAction, &QAction::triggered, this, [this]() {
    //     qDebug() << "Kliknięto moja opcja";
    // });
    // menu->addAction(customAction);

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
