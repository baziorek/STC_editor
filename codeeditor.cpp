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
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include "codeeditor.h"
#include "linenumberarea.h"
#include "stcsyntaxhighlighter.h"
#include "ui/cppcompilerdialog.h"
#include "utils/diffcalculation.h"
#include "types/CodeBlock.h"
#include "utils/FileEncodingHandler.h"
#include "stcsyntaxpatterns.h"


namespace
{
constexpr int spacesPerTab = 4;
} // namespace

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent), networkManager{new QNetworkAccessManager(this)}, lineNumberArea{new LineNumberArea(this)}, fileEncodingHandler{std::make_unique<FileEncodingHandler>()}
{
    setAcceptDrops(true);
    setMouseTracking(true);
    document()->setModified(false);

    registerShortcuts();

    connectSignalsWithSlots();


    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    STCSyntaxHighlighter *highlighter = new STCSyntaxHighlighter(document()); // it does not leak
}

void CodeEditor::newEmptyFile()
{
    clear();
    setFileName("");

    document()->setModified(false);

    originalLines.clear();
    modifiedLines.clear();

    fileModificationTime = {};
    lastChangeTime = QDateTime::currentDateTime();

    currentLine = -1;

    codeBlocks = {};

    fileEncodingHandler = std::make_unique<FileEncodingHandler>();
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

void CodeEditor::connectSignalsWithSlots()
{
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::totalLinesCountChanged);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CodeEditor::onScrollChanged);
    connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &CodeEditor::fileChanged);

    connect(this, &CodeEditor::textChanged, this, [this]() {
        this->lastChangeTime = QDateTime::currentDateTime();
        updateDiffWithOriginal();
    });
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::onCursorPositionChanged);
    connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::onContentsChange);
}

int CodeEditor::lineNumberAreaWidth()
{
    const int maxLine = linesCount();
    const int digits4LineNumber = std::log10(maxLine) + 1;

    const int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits4LineNumber;
    return space;
}

bool CodeEditor::noUnsavedChanges() const
{
    if (document()->isModified())
    {
        return false;
    }

    const auto currentlyVisibleText = toPlainText();
    if (getFileName().isEmpty())
    {
        return currentlyVisibleText.isEmpty();
    }

    try
    {
        const auto fileContent = fileEncodingHandler->loadFile(getFileName());
        return fileContent == currentlyVisibleText;
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(const_cast<CodeEditor*>(this), "Checking if no unsaved changes failed!", e.what());
        return false;
    }
}

const QString CodeEditor::getFileName() const
{
    return document()->baseUrl().toLocalFile();
}

void CodeEditor::setFileName(const QString &newFileName)
{
    document()->setBaseUrl(QUrl::fromLocalFile(newFileName));
    setDocumentTitle(QFileInfo(newFileName).fileName());

    // File watching
    if (!newFileName.isEmpty() && QFile::exists(newFileName))
    {
        enableWatchingOfFile(newFileName);
    }
    else if (! fileWatcher.files().isEmpty())
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

    if (loadFileContentDistargingCurrentContent(getFileName()))
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
    try
    {
        const auto fileContent = fileEncodingHandler->loadFile(fileName);
        setPlainText(fileContent);

        document()->setModified(false);

        trackOriginalVersionOfFile(fileName);

        setFileName(fileName);

        analizeEntireDocumentDetectingCodeBlocks();

        return true;
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(this, tr("Loading file error"), QString::fromStdString(e.what()));
        return false;
    }
}

bool CodeEditor::saveEntireContent2File(const QString &fileName)
{
    QFile outputFile(fileName);
    outputFile.open(QIODeviceBase::WriteOnly);
    setFileName(fileName);

    auto savedNumberOfBytes = outputFile.write(toPlainText().toUtf8());
    if (savedNumberOfBytes > -1)
    {
        markAsSaved();
        return true;
    }
    return false;
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

    for (QShortcut* s : findChildren<QShortcut*>())
    {
        QString desc = s->objectName();
        if (desc.isEmpty())
            desc = "Editor shortcut";
        result.insert(desc, s->key());
    }

    return result;
}

void CodeEditor::contextMenuEvent(QContextMenuEvent* event)
{
    moveCursorToClickPosition(event->pos());
    QMenu* menu = createStandardContextMenu();

    const QTextCursor selection = textCursor();

    if (selection.hasSelection())
    {
        addCaseConversionActions(menu, selection);
        addWordFormatActions(menu, selection);
        addMultiLineSelectionActions(menu, selection);
    }
    else
    {
        addTagRemovalActionIfInsideTag(menu);
        addCodeBlockActionsIfApplicable(menu, event->pos());
        addImgTagActionsIfApplicable(menu);
        addPktTagActionsIfApplicable(menu);
        addCsvTagActionsIfApplicable(menu);
        addAnchorTagActionsIfApplicable(menu);
        addDivTagActionsIfApplicable(menu);
        addHeaderTagActionsIfApplicable(menu, event->pos());
    }

    menu->exec(event->globalPos());
    delete menu;
}

void CodeEditor::addImgTagActionsIfApplicable(QMenu* menu)
{
    using namespace stc::syntax;
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search all [img ...] tags in the current line
    QRegularExpressionMatchIterator it = imgRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        if (offset >= tagStart && offset <= tagEnd)
        {
            QString imgTag = match.captured(0);
            bool hasSrc = imgAttributeSrcRe.match(imgTag).hasMatch();
            bool hasAlt = imgAttributeAltRe.match(imgTag).hasMatch();
            bool hasOpis = imgAttributeDescRe.match(imgTag).hasMatch();
            bool hasAutofit = imgAttributeAutofitRe.match(imgTag).hasMatch();

            // Add a separator before image tag actions
            menu->addSeparator();

            // ALT attribute action
            QAction* toggleAlt = new QAction(tr("alt attribute"), this);
            toggleAlt->setCheckable(true);
            toggleAlt->setChecked(hasAlt);
            connect(toggleAlt, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = imgTag;
                if (!hasAlt)
                {
                    // Add alt
                    newTag.insert(newTag.length() - 1, " alt=\"\"");
                }
                else
                {
                    // Remove alt
                    newTag.replace(QRegularExpression("\\s*alt=\\\"[^\\\"]*\\\""), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleAlt);

            // OPIS attribute action
            QAction* toggleOpis = new QAction(tr("opis attribute"), this);
            toggleOpis->setCheckable(true);
            toggleOpis->setChecked(hasOpis);
            connect(toggleOpis, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = imgTag;
                if (!hasOpis)
                {
                    // Add opis
                    newTag.insert(newTag.length() - 1, " opis=\"\"");
                }
                else
                {
                    // Remove opis
                    newTag.replace(QRegularExpression("\\s*opis=\\\"[^\\\"]*\\\""), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleOpis);

            // AUTOFIT attribute action
            QAction* toggleAutofit = new QAction(tr("autofit attribute"), this);
            toggleAutofit->setCheckable(true);
            toggleAutofit->setChecked(hasAutofit);
            connect(toggleAutofit, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = imgTag;
                if (!hasAutofit)
                {
                    // Add autofit
                    newTag.insert(newTag.length() - 1, " autofit");
                }
                else
                {
                    // Remove autofit
                    newTag.replace(QRegularExpression("\\s*autofit"), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleAutofit);

            // src is required and should not be removed
            // Only handle the first [img ...] tag in the line
            return;
        }
    }
}

void CodeEditor::addPktTagActionsIfApplicable(QMenu* menu)
{
    using namespace stc::syntax;
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search all [pkt ...] tags in the current line
    QRegularExpressionMatchIterator it = pktOpenRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        if (offset >= tagStart && offset <= tagEnd)
        {
            QString pktTag = match.captured(0);
            bool hasExt = pktTag.contains(QRegularExpression("\\bext\\b"));

            // Add a separator before pkt tag actions
            menu->addSeparator();

            // EXT attribute action
            QAction* toggleExt = new QAction(tr("ext attribute"), this);
            toggleExt->setCheckable(true);
            toggleExt->setChecked(hasExt);
            connect(toggleExt, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = pktTag;
                if (!hasExt)
                {
                    // Add ext
                    newTag.insert(newTag.length() - 1, " ext");
                }
                else
                {
                    // Remove ext
                    newTag.replace(QRegularExpression("\\s*ext\\b"), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleExt);
            // Only handle the first [pkt ...] tag in the line
            return;
        }
    }
}

void CodeEditor::addCsvTagActionsIfApplicable(QMenu* menu)
{
    using namespace stc::syntax;
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search all [csv ...] tags in the current line
    QRegularExpressionMatchIterator it = csvOpenRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        if (offset >= tagStart && offset <= tagEnd)
        {
            QString csvTag = match.captured(0);
            bool hasExtended = csvTag.contains(QRegularExpression("\\bextended\\b"));
            bool hasHeader = csvTag.contains(QRegularExpression("\\bheader\\b"));

            // Add a separator before csv tag actions
            menu->addSeparator();

            // EXTENDED attribute action
            QAction* toggleExtended = new QAction(tr("extended attribute"), this);
            toggleExtended->setCheckable(true);
            toggleExtended->setChecked(hasExtended);
            connect(toggleExtended, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = csvTag;
                if (!hasExtended)
                {
                    // Add extended
                    newTag.insert(newTag.length() - 1, " extended");
                }
                else
                {
                    // Remove extended
                    newTag.replace(QRegularExpression("\\s*extended\\b"), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleExtended);

            // HEADER attribute action
            QAction* toggleHeader = new QAction(tr("header attribute"), this);
            toggleHeader->setCheckable(true);
            toggleHeader->setChecked(hasHeader);
            connect(toggleHeader, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = csvTag;
                if (!hasHeader)
                {
                    // Add header
                    newTag.insert(newTag.length() - 1, " header");
                }
                else
                {
                    // Remove header
                    newTag.replace(QRegularExpression("\\s*header\\b"), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleHeader);
            // Only handle the first [csv ...] tag in the line
            return;
        }
    }
}

void CodeEditor::addAnchorTagActionsIfApplicable(QMenu* menu)
{
    using namespace stc::syntax;
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search all [a href=...] tags in the current line
    QRegularExpressionMatchIterator it = anchorRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        if (offset >= tagStart && offset <= tagEnd)
        {
            QString anchorTag = match.captured(0);
            bool hasName = anchorTag.contains(QRegularExpression("name=\"[^\"]*\""));

            // Add a separator before anchor tag actions
            menu->addSeparator();

            // NAME attribute action
            QAction* toggleName = new QAction(tr("name attribute"), this);
            toggleName->setCheckable(true);
            toggleName->setChecked(hasName);
            connect(toggleName, &QAction::triggered, this, [=, this]() {
                QTextCursor c = textCursor();
                c.beginEditBlock();
                QString newTag = anchorTag;
                if (!hasName)
                {
                    // Add name=""
                    newTag.insert(newTag.length() - 1, " name=\"\"");
                }
                else
                {
                    // Remove name
                    newTag.replace(QRegularExpression("\\s*name=\\\"[^\\\"]*\\\""), "");
                }
                c.setPosition(cursor.block().position() + tagStart);
                c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                c.insertText(newTag);
                c.endEditBlock();
            });
            menu->addAction(toggleName);
            // Only handle the first [a href=...] tag in the line
            return;
        }
    }
}

void CodeEditor::addDivTagActionsIfApplicable(QMenu* menu)
{
    using namespace stc::syntax;
    QTextCursor cursor = textCursor();
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search all [div ...] tags in the current line
    QRegularExpression divTagRe(R"(\[div(\s+class=\"(tip|uwaga)\")?\])");
    QRegularExpressionMatchIterator it = divTagRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        if (offset >= tagStart && offset <= tagEnd)
        {
            QString divTag = match.captured(0);
            bool isPlain = !divTag.contains("class=\"");
            bool isTip = divTag.contains("class=\"tip\"");
            bool isUwaga = divTag.contains("class=\"uwaga\"");

            // Add a separator before div tag actions
            menu->addSeparator();

            // Action: [div] (no class)
            if (!isPlain)
            {
                QAction* plainDiv = new QAction(tr("Convert to [div]"), this);
                plainDiv->setIcon(QIcon::fromTheme("format-justify-fill"));
                connect(plainDiv, &QAction::triggered, this, [=, this]() {
                    QTextCursor c = textCursor();
                    c.beginEditBlock();
                    QString newTag = "[div]";
                    c.setPosition(cursor.block().position() + tagStart);
                    c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                    c.insertText(newTag);
                    c.endEditBlock();
                });
                menu->addAction(plainDiv);
            }

            // Action: [div class="tip"]
            if (! isTip)
            {
                QAction* tipDiv = new QAction(tr("Convert to [div class=\"tip\"]"), this);
                tipDiv->setIcon(QIcon::fromTheme("dialog-information"));
                connect(tipDiv, &QAction::triggered, this, [=, this]() {
                    QTextCursor c = textCursor();
                    c.beginEditBlock();
                    QString newTag = "[div class=\"tip\"]";
                    c.setPosition(cursor.block().position() + tagStart);
                    c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                    c.insertText(newTag);
                    c.endEditBlock();
                });
                menu->addAction(tipDiv);
            }

            // Action: [div class="uwaga"]
            if (! isUwaga)
            {
                QAction* uwagaDiv = new QAction(tr("Convert to [div class=\"uwaga\"]"), this);
                uwagaDiv->setIcon(QIcon::fromTheme("dialog-warning"));
                connect(uwagaDiv, &QAction::triggered, this, [=, this]() {
                    QTextCursor c = textCursor();
                    c.beginEditBlock();
                    QString newTag = "[div class=\"uwaga\"]";
                    c.setPosition(cursor.block().position() + tagStart);
                    c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                    c.insertText(newTag);
                    c.endEditBlock();
                });
                menu->addAction(uwagaDiv);
            }

            // Only handle the first [div ...] tag in the line
            return;
        }
    }
}

void CodeEditor::moveCursorToClickPosition(const QPoint& pos)
{
    QTextCursor clickCursor = cursorForPosition(pos);
    QTextCursor current = textCursor();

    if (current.hasSelection())
    {
        int clickPos = clickCursor.position();
        if (clickPos < current.selectionStart() || clickPos > current.selectionEnd())
            setTextCursor(clickCursor);
    }
    else
    {
        setTextCursor(clickCursor);
    }
}

void CodeEditor::addCaseConversionActions(QMenu* menu, const QTextCursor& selection)
{
    const QString text = selection.selectedText();

    menu->addSeparator();

    if (!text.isUpper())
    {
        QAction* upper = new QAction(QIcon::fromTheme("format-text-uppercase"), "To UPPER CASE", this);
        connect(upper, &QAction::triggered, this, [this]() {
            auto cursor = textCursor();
            cursor.insertText(cursor.selectedText().toUpper());
        });
        menu->addAction(upper);
    }

    if (!text.isLower())
    {
        QAction* lower = new QAction(QIcon::fromTheme("format-text-lowercase"), "To lower case", this);
        connect(lower, &QAction::triggered, this, [this]() {
            auto cursor = textCursor();
            cursor.insertText(cursor.selectedText().toLower());
        });
        menu->addAction(lower);
    }
}

void CodeEditor::addWordFormatActions(QMenu* menu, const QTextCursor& selection)
{
    const QString text = selection.selectedText();
    if (text.contains(QRegularExpression("\\s"))) return;

    if (text.contains('_'))
    {
        QAction* toCamel = new QAction(QIcon::fromTheme("format-text-italic"), "To camelCase", this);
        connect(toCamel, &QAction::triggered, this, [this, text]() {
            QStringList parts = text.split('_', Qt::SkipEmptyParts);
            for (int i = 1; i < parts.size(); ++i)
                parts[i][0] = parts[i][0].toUpper();
            textCursor().insertText(parts.join(""));
        });
        menu->addAction(toCamel);
    }
    else
    {
        QAction* toSnake = new QAction(QIcon::fromTheme("format-text-bold"), "To snake_case", this);
        connect(toSnake, &QAction::triggered, this, [this, text]() {
            QString result;
            for (QChar ch : text)
            {
                if (ch.isUpper())
                    result += QString{"_"} + ch.toLower();
                else
                    result += ch;
            }
            textCursor().insertText(result);
        });
        menu->addAction(toSnake);
    }
}

void CodeEditor::addMultiLineSelectionActions(QMenu* menu, const QTextCursor& selection)
{
    const int start = selection.selectionStart();
    const int end = selection.selectionEnd();
    const int startLine = document()->findBlock(start).blockNumber();
    const int endLine = document()->findBlock(end).blockNumber();

    if (endLine <= startLine)
        return;

    menu->addSeparator();

    QAction* numbered = new QAction(QIcon::fromTheme("format-list-numbered"), "Add numeration: 1., 2., 3. ...", this);
    connect(numbered, &QAction::triggered, this, [=, this]() {
        for (int i = startLine, n = 1; i <= endLine; ++i, ++n)
        {
            QTextBlock block = document()->findBlockByNumber(i);
            QTextCursor c(block);
            c.movePosition(QTextCursor::StartOfBlock);
            c.insertText(QString::number(n) + ". ");
        }
    });
    menu->addAction(numbered);

    QAction* bulleted = new QAction(QIcon::fromTheme("format-list-unordered"), "Add bullet points", this);
    connect(bulleted, &QAction::triggered, this, [=, this]() {
        for (int i = startLine; i <= endLine; ++i)
        {
            QTextBlock block = document()->findBlockByNumber(i);
            QTextCursor c(block);
            c.movePosition(QTextCursor::StartOfBlock);
            c.insertText("- ");
        }
    });
    menu->addAction(bulleted);

    QAction* joinLines = new QAction(QIcon::fromTheme("insert-text"), "Join lines with space", this);
    connect(joinLines, &QAction::triggered, this, [this]() {
        auto c = textCursor();
        QString joined = c.selectedText().replace(QChar::ParagraphSeparator, " ");
        c.insertText(joined);
    });
    menu->addAction(joinLines);

    // Sort ascending
    QAction* sortAsc = new QAction(QIcon::fromTheme("view-sort-ascending"), "Sort lines ascending", this);
    connect(sortAsc, &QAction::triggered, this, [this, startLine, endLine]() {
        sortLinesInRange(startLine, endLine, /*ascending=*/true);
    });
    menu->addAction(sortAsc);

    // Sort descending
    QAction* sortDesc = new QAction(QIcon::fromTheme("view-sort-descending"), "Sort lines descending", this);
    connect(sortDesc, &QAction::triggered, this, [this, startLine, endLine]() {
        sortLinesInRange(startLine, endLine, /*ascending=*/false);
    });
    menu->addAction(sortDesc);
}
void CodeEditor::sortLinesInRange(int startLine, int endLine, bool ascending)
{
    QStringList lines;

    for (int i = startLine; i <= endLine; ++i)
    {
        QTextBlock block = document()->findBlockByNumber(i);
        if (block.isValid())
            lines << block.text();
    }

    std::sort(lines.begin(), lines.end(), [ascending](const QString& a, const QString& b) {
        return ascending
                   ? QString::compare(a, b, Qt::CaseInsensitive) < 0
                   : QString::compare(a, b, Qt::CaseInsensitive) > 0;
    });

    QTextCursor cursor(document()->findBlockByNumber(startLine));
    cursor.beginEditBlock();

    for (int i = startLine; i <= endLine; ++i)
    {
        QTextBlock block = document()->findBlockByNumber(i);
        if (block.isValid())
        {
            QTextCursor lineCursor(block);
            lineCursor.select(QTextCursor::LineUnderCursor);
            lineCursor.removeSelectedText();
            lineCursor.insertText(lines[i - startLine]);
        }
    }

    cursor.endEditBlock();
}

void CodeEditor::addTagRemovalActionIfInsideTag(QMenu* menu)
{
    const QStringList tags = { "code", "cpp", "py", "b", "u", "i", "h1", "h2", "h3", "h4", "run" };

    QTextCursor cursor = textCursor();
    QString text = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    for (const QString& tag : tags)
    {
        QRegularExpression re(QString(R"(\[%1\](.*?)\[/%1\])").arg(tag));
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext())
        {
            QRegularExpressionMatch m = it.next();
            if (offset >= m.capturedStart(1) && offset <= m.capturedEnd(1))
            {
                QAction* remove = new QAction(QIcon::fromTheme("edit-delete"), QString("Remove [%1]").arg(tag), this);
                connect(remove, &QAction::triggered, this, [=, this]() {
                    QTextCursor c = textCursor();
                    int lineOffset = c.position() - c.block().position();
                    QRegularExpressionMatchIterator it2 = re.globalMatch(c.block().text());
                    while (it2.hasNext())
                    {
                        QRegularExpressionMatch m2 = it2.next();
                        if (lineOffset >= m2.capturedStart(1) && lineOffset <= m2.capturedEnd(1))
                        {
                            QTextCursor lineCursor(c.block());
                            lineCursor.setPosition(c.block().position() + m2.capturedStart());
                            lineCursor.setPosition(c.block().position() + m2.capturedEnd(), QTextCursor::KeepAnchor);
                            lineCursor.insertText(m2.captured(1));
                            break;
                        }
                    }
                });
                menu->addSeparator();
                menu->addAction(remove);
                return;
            }
        }
    }
}

void CodeEditor::addCodeBlockActionsIfApplicable(QMenu* menu, const QPoint& pos)
{
    const int cursorPos = cursorForPosition(pos).position();
    if (auto maybeBlock = selectEnclosingCodeBlock(cursorPos); maybeBlock.has_value())
    {
        auto cursor = maybeBlock->cursor;
        auto tag = maybeBlock->tag;

        menu->addSeparator();

        QAction* selectAll = new QAction(QIcon::fromTheme("edit-select-all"), "Select this source code", this);
        connect(selectAll, &QAction::triggered, this, [=, this]() {
            setTextCursor(cursor);
        });
        menu->addAction(selectAll);

        if (tag == "cpp")
        {
            QAction* format = new QAction(QIcon::fromTheme("tools-wizard"), "Format C++ with clang-format", this);
            connect(format, &QAction::triggered, this, [=, this]() mutable {
                QString raw = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
                QString formatted = formatCppWithClang(raw);
                if (!formatted.isEmpty())
                    cursor.insertText(formatted.replace(QChar::LineSeparator, "\n"));
                else
                    QMessageBox::warning(this, "clang-format", "Formatting failed or clang-format not available.");
            });
            menu->addAction(format);

            QAction* compile = new QAction(QIcon::fromTheme("applications-development"), "Compile C++ with g++", this);
            connect(compile, &QAction::triggered, this, [=, this]() {
                QString raw = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
                auto* dlg = new CppCompilerDialog(raw, this);
                dlg->exec();
            });
            menu->addAction(compile);
        }
    }
}

QString CodeEditor::formatCppWithClang(const QString& code)
{
    constexpr const char clangFormatConfigDefaultName[] = ".clang-format";
    constexpr const char clangFormatDefaultStyleIfNoConfigFound[] = "-style=LLVM";

    QStringList args;
    const QString sourceFilePath = getFileName();

    // Try to find a clang-format configuration file in the current or parent directories
    QFileInfo fileInfo(sourceFilePath);
    QDir dir = fileInfo.dir();

    bool foundClangFormatFile = false;
    while (dir.exists())
    {
        if (dir.exists(clangFormatConfigDefaultName))
        {
            foundClangFormatFile = true;
            break;
        }

        // Stop if we reach the root directory
        if (!dir.cdUp())
            break;
    }

    if (! foundClangFormatFile)
        args << clangFormatDefaultStyleIfNoConfigFound;  // fallback style if no config file found

    QProcess clang;
    clang.start("clang-format", args);

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
    if (getFileName().isEmpty())
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

        QColor lineColor = QColor(Qt::gray).lighter(60);

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

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (isControlOnly(event))
    {
        if (event->key() == Qt::Key_B)  // TODO: This shortcut should be set up in constructor, but it is not working
        {
            emit shortcutPressed_bold();
            return;
        }
        if (event->key() == Qt::Key_V)
        {
            if (!(event->modifiers() & Qt::ShiftModifier))
            {
                if (handlePasteWithLinkWrapping())
                    return; // link pasted, stop propagation
            }
        }
    }

    if (event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier))
    {
        handleTabIndent();
        return;
    }

    if (event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && (event->modifiers() & Qt::ShiftModifier)))
    {
        handleTabUnindent();
        return;
    }

    QPlainTextEdit::keyPressEvent(event); // Default behavior
}

bool CodeEditor::isControlOnly(QKeyEvent* event) const
{
    return (event->modifiers() & Qt::ControlModifier) &&
           !(event->modifiers() & ~Qt::ControlModifier);
}

bool CodeEditor::handlePasteWithLinkWrapping()
{
    const QString clipboardText = QGuiApplication::clipboard()->text().trimmed();
    if (!isLink(clipboardText))
        return false;

    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        const QString selectedText = cursor.selectedText();
        const QString wrapped = QString(R"([a href="%1" name="%2"])").arg(clipboardText, selectedText);
        cursor.insertText(wrapped);
    }
    else
    {
        // Insert link and start downloading of page title:
        const QString linkText = QString(R"([a href="%1"])").arg(clipboardText);
        int insertPos = cursor.position();
        cursor.insertText(linkText);

        fetchAndInsertTitle(clipboardText, insertPos);
    }

    return true;
}
void CodeEditor::fetchAndInsertTitle(const QString& url, int insertedPos)
{
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    QNetworkReply* reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        reply->deleteLater();

        QTextDocument* doc = this->document();
        QTextCursor cursor(doc);
        cursor.setPosition(insertedPos);

        // Find position of `[a href="url"]` — and limit to 200 characters
        const int searchLen = 200;
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, searchLen);
        const QString fragment = cursor.selectedText();

        QRegularExpression rx(R"(\[a\s+href=")" + QRegularExpression::escape(url) + R"("\])");
        QRegularExpressionMatch match = rx.match(fragment);
        if (!match.hasMatch()) // link not found
        {
            return;
        }

        const int relativeStart = match.capturedStart();
        const int absStart = insertedPos + relativeStart;
        const int absEnd = absStart + match.capturedLength();

        // set coursor on [a href="..."]
        QTextCursor replaceCursor(doc);
        replaceCursor.setPosition(absStart);
        replaceCursor.setPosition(absEnd, QTextCursor::KeepAnchor);

        if (reply->error() != QNetworkReply::NoError)
        {
            replaceCursor.insertText(QString(R"([a href="%1" name="LINK NIE ISTNIEJE"])").arg(url));
            int lineNumber = doc->findBlock(absStart).blockNumber();
            emit linkTitleFetchFailed(url, lineNumber + 1, reply->errorString());
            return;
        }

        const QByteArray html = reply->readAll();

        static const QRegularExpression titleRx(R"(<title[^>]*>(.*?)</title>)",
                                                QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch tMatch = titleRx.match(QString::fromUtf8(html));

        QString title = url; // fallback
        if (tMatch.hasMatch())
        {
            title = tMatch.captured(1).simplified();
        }

        const QString updated = QString(R"([a href="%1" name="%2"])").arg(url, title);
        replaceCursor.insertText(updated);
    });
}


void CodeEditor::handleTabIndent()
{
    QTextCursor cursor = textCursor();

    if (cursor.hasSelection())
    {
        applyToSelectedBlocks([this](QTextCursor& blockCursor) {
            blockCursor.insertText(QString(spacesPerTab, ' '));
        });
    }
    else
    {
        insertPlainText(QString(spacesPerTab, ' '));
    }
}
void CodeEditor::handleTabUnindent()
{
    QTextCursor cursor = textCursor();

    if (!cursor.hasSelection())
        return;

    applyToSelectedBlocks([this](QTextCursor& blockCursor) {
        blockCursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QString firstChar = blockCursor.selectedText();

        if (firstChar == "\t")
        {
            blockCursor.removeSelectedText();
        }
        else
        {
            blockCursor = QTextCursor(blockCursor.block()); // reset to block start
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
    });
}
void CodeEditor::applyToSelectedBlocks(const std::function<void(QTextCursor&)>& callback)
{
    QTextCursor cursor = textCursor();
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
        callback(blockCursor);
    }
    cursor.endEditBlock();
}


void CodeEditor::fileChanged(const QString &path)
{
    // stop watching file to avoid multiple signals
    fileWatcher.removePath(path);

    // delayed reaction
    QTimer::singleShot(300, this, [this, path]() {
        const QByteArray currentContent = toPlainText().toUtf8();

        try
        {
            const QString newContent = fileEncodingHandler->loadFile(path);

            if (newContent == currentContent)
            {
                enableWatchingOfFile(path);
                return;
            }
        }
        catch (const std::exception& e)
        {
            QMessageBox::warning(const_cast<CodeEditor*>(this), "Checking if no unsaved changes failed!", e.what());
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
            reloadFromFile(/*discardChanges=*/true);
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

    // moving cursor position to position where we dropped something
    QPoint dropPosition = event->position().toPoint();
    QTextCursor cursor = cursorForPosition(dropPosition);
    setTextCursor(cursor);

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
        if (fileEncodingHandler->isProbablyTextFile(localPath))
        {
            const QString fileContent = fileEncodingHandler->loadFile(localPath).trimmed();
            const QStringList cppExtensions = {"c", "cpp", "h", "hpp", "cc", "cxx", "hxx"};
            if (cppExtensions.contains(fileInfo.suffix().toLower()))
            {
                insertPlainText("[cpp]" + fileContent + "[/cpp]");
            }
            else // other text file type
            {
                insertPlainText("[code]" + fileContent + "[/code]");
            }
        }
        // 2. Add link to image file
        else if (QImageReader::supportedImageFormats().contains(suffix.toUtf8()))
        {
            QTextCursor cursor = textCursor();
            cursor.insertText(QString(R"([img src="%1" alt="%2"])").arg(localPath, fileInfo.baseName()));
        }
    }

    if (0 == urls.size()) // no urls:
    {
        insertPlainText(event->mimeData()->text());
        event->acceptProposedAction();
    }

    event->acceptProposedAction();
}

void CodeEditor::mouseMoveEvent(QMouseEvent* event)
{
    // Do not show tooltip while selecting with the left mouse button
    if (const bool isSelectingWithLeftMouseButtonActive = event->buttons() & Qt::LeftButton)
    {
        clearTooltipState();
        QPlainTextEdit::mouseMoveEvent(event);
        return;
    }

    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);

    auto imagePathOpt = extractImagePath("does not matter", cursor);
    if (!imagePathOpt)
    {
        return;
    }

    auto imagePath = *imagePathOpt;
    if (!imagePath.isEmpty())
    {
        if (isLocalImageFile(imagePath))
        {
            showLocalImageTooltip(imagePath, event->globalPosition().toPoint());
            return;
        }
        else if (isLink(imagePath))
        {
            showWebLinkPreview(imagePath, event->globalPosition().toPoint());
            return;
        }
        else // file not found or unsupported format
        {
            if (imagePath != lastTooltipImagePath)
            {
                lastTooltipImagePath = imagePath;
                QString errorMessage = QString("2Error: Image file not found or invalid:<br/>%1").arg(imagePath);
                QToolTip::showText(event->globalPosition().toPoint(), errorMessage, this);
            }
            return;
        }
    }
    // No match – hide tooltip and call base method
    clearTooltipState();
    QPlainTextEdit::mouseMoveEvent(event);
}

std::optional<QString> CodeEditor::extractImagePath(const QString& /*text*/, const QTextCursor& cursor) const
{
    static QRegularExpression imgRegex(R"__(
        \[img\s+
        (?:
            (?:src="([^"]+)")|
            (?:alt="[^"]*")|
            (?:opis="[^"]*")|
            (?:autofit\b)
        )
        (?:\s+
            (?:
                (?:src="([^"]+)")|
                (?:alt="[^"]*")|
                (?:opis="[^"]*")|
                (?:autofit\b)
            )
        )*
        \s*\])__", QRegularExpression::ExtendedPatternSyntaxOption);

    QTextBlock block = cursor.block();
    const QString blockText = block.text();
    QRegularExpressionMatch match = imgRegex.match(blockText);

    if (match.hasMatch())
    {
        // Find first non-empty src attribute value
        QString imagePath;
        for (int i = 1; i <= match.lastCapturedIndex(); ++i)
        {
            if (!match.captured(i).isEmpty())
            {
                imagePath = match.captured(i);
                return imagePath;
            }
        }
    }
    return std::nullopt;
}

bool CodeEditor::isLocalImageFile(const QString& path) const
{
    QFileInfo fi(path);
    return fi.exists() && fi.isFile() &&
           QImageReader::supportedImageFormats().contains(fi.suffix().toLower().toUtf8());
}

bool CodeEditor::isLink(const QString& path) const
{
    return path.startsWith("http://", Qt::CaseInsensitive) || path.startsWith("https://", Qt::CaseInsensitive);
}


void CodeEditor::showLocalImageTooltip(const QString& path, const QPoint& globalPos)
{
    if (path == lastTooltipImagePath)   // avoid flicker
        return;
    lastTooltipImagePath = path;

    QFileInfo fi(path);
    if (!fi.exists() || !QImageReader::supportedImageFormats().contains(fi.suffix().toLower().toUtf8())) {
        QToolTip::showText(globalPos, QString("Error: Image not found or invalid:<br/>%1").arg(path), this);
        return;
    }

    QImage img(path);
    if (img.isNull()) {
        QToolTip::showText(globalPos, QString("Error: Cannot load image:<br/>%1").arg(path), this);
        return;
    }

    const QSize preview = img.size().boundedTo({200,150});
    QPixmap pix = QPixmap::fromImage(img.scaled(preview, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QString html = QString("<b>%1</b><br/>"
                           "<img src=\"%2\" height=\"%3\"/><br/>"
                           "<i>%4×%5 px</i><br/>"
                           "Last modified: %6"
                           ).arg(fi.fileName())
                           .arg(path)
                           .arg(preview.height())
                           .arg(img.width()).arg(img.height())
                           .arg(fi.lastModified().toString(Qt::ISODate));

    QToolTip::showText(globalPos, html, this);
}

void CodeEditor::showWebLinkPreview(const QString& url, const QPoint& globalPos)
{
    if (url == lastTooltipImagePath)
        return;

    lastTooltipImagePath = url;
    QToolTip::showText(globalPos, "Loading preview…", this);

    QNetworkRequest request{QUrl(url)};
    QNetworkReply* reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QToolTip::showText(globalPos, QString("Error: %1").arg(reply->errorString()), this);
            return;
        }

        QByteArray data = reply->readAll();
        QImage image;
        if (!image.loadFromData(data)) {
            QToolTip::showText(globalPos, "Received data is not a valid image.", this);
            return;
        }

        const QSize preview = image.size().boundedTo({200, 150});
        QImage scaled = image.scaled(preview, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QByteArray imageBytes;
        QBuffer buffer(&imageBytes);
        buffer.open(QIODevice::WriteOnly);
        scaled.save(&buffer, "PNG");

        const QString base64 = QString::fromLatin1(imageBytes.toBase64());

        const QString html = QString("<img src=\"data:image/png;base64,%1\" height=\"%2\"/><br/>"
                                     "<i>%3 × %4 px</i><br/>"
                                     "From: %5")
                                     .arg(base64)
                                     .arg(preview.height())
                                     .arg(image.width())
                                     .arg(image.height())
                                     .arg(url);

        QToolTip::showText(globalPos, html, this);
    });
}

void CodeEditor::clearTooltipState()
{
    lastTooltipImagePath.clear();
    QToolTip::hideText();
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
    const QSet<int> newDiff = DiffCalculation::calculateModifiedLines(originalLines, currentLines);

    if (newDiff != modifiedLines)
    {
        modifiedLines = newDiff;
        emit numberOfModifiedLinesChanged(modifiedLines.size());
        if (0 == modifiedLines.size()) // CTRL + Z or CTRL + Y can remove changes to file
        {
            document()->setModified(false);
        }
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
    fileModificationTime = QFileInfo(getFileName()).lastModified();
    lineNumberArea->update();

    emit numberOfModifiedLinesChanged(0);

    document()->setModified(false);
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

void CodeEditor::addHeaderTagActionsIfApplicable(QMenu* menu, const QPoint& pos)
{
    QTextCursor cursor = cursorForPosition(pos);
    QString line = cursor.block().text();
    int offset = cursor.position() - cursor.block().position();

    // Search for opening [hN] tags in the line
    QRegularExpression openHeaderTagRe(R"(\[(h[1-6])\])");
    QRegularExpressionMatchIterator it = openHeaderTagRe.globalMatch(line);
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int tagStart = match.capturedStart(0);
        int tagEnd = match.capturedEnd(0);
        QString currentTag = match.captured(1); // e.g. h1
        if (offset >= tagStart && offset <= tagEnd)
        {
            // Search for closing tag in the same line
            QRegularExpression closeHeaderTagRe(QString(R"(\[/%1\])").arg(currentTag));
            QRegularExpressionMatch closeMatch = closeHeaderTagRe.match(line, tagEnd);
            if (closeMatch.hasMatch())
            {
                int closeTagStart = closeMatch.capturedStart(0);
                int closeTagEnd = closeMatch.capturedEnd(0);

                // Add separator
                menu->addSeparator();
                // Add conversion actions to other header levels
                for (int i = 1; i <= 6; ++i)
                {
                    QString newTag = QString("h%1").arg(i);
                    if (newTag == currentTag)
                        continue;
                    QString actionText = QString("Convert to [%1]").arg(newTag);
                    QAction* act = new QAction(actionText, this);
                    connect(act, &QAction::triggered, this, [=, this]() {
                        QTextCursor c = textCursor();
                        c.beginEditBlock();
                        // Replace opening tag
                        c.setPosition(cursor.block().position() + tagStart);
                        c.setPosition(cursor.block().position() + tagEnd, QTextCursor::KeepAnchor);
                        c.insertText("[" + newTag + "]");
                        // Replace closing tag
                        c.setPosition(cursor.block().position() + closeTagStart);
                        c.setPosition(cursor.block().position() + closeTagEnd, QTextCursor::KeepAnchor);
                        c.insertText(QString("[") + "/" + newTag + "]");
                        c.endEditBlock();
                    });
                    menu->addAction(act);
                }
                // Handle only the first matching tag in the line
                return;
            }
        }
    }
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
    handleCodeBlockDetectionOnChange(position);
}

void CodeEditor::handleCodeBlockDetectionOnChange(int position) // emit codeBlocksChanged
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

void CodeEditor::mousePressEvent(QMouseEvent* event)
{
    if (isCtrlLeftClick(event))
    {
        const QTextCursor cursor = cursorForPosition(event->pos());
        const QTextBlock block = cursor.block();
        const int clickPosInBlock = cursor.position() - block.position();

        if (tryOpenLinkAtPosition(block.text(), clickPosInBlock))
            return; // link found and opened
    }

    QPlainTextEdit::mousePressEvent(event); // default behavior
}

bool CodeEditor::isCtrlLeftClick(QMouseEvent* event) const
{
    return event->button() == Qt::LeftButton &&
           (event->modifiers() & Qt::ControlModifier);
}

bool CodeEditor::tryOpenLinkAtPosition(const QString& text, int posInBlock)
{
    static const QRegularExpression hrefRegex(
        R"__(\[a\s+href="([^"]+)"(?:\s+name="([^"]*)")?\])__",
        QRegularExpression::CaseInsensitiveOption);

    auto matchIterator = hrefRegex.globalMatch(text);
    while (matchIterator.hasNext())
    {
        const QRegularExpressionMatch match = matchIterator.next();
        const int start = match.capturedStart(0);
        const int end = match.capturedEnd(0);

        if (posInBlock >= start && posInBlock <= end)
        {
            const QString url = match.captured(1).trimmed();
            QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
            return true;
        }
    }

    return false;
}
