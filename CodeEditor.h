/** @file CodeEditor.h
 * @brief Declaration of CodeEditor, the central text-editing widget of STC_editor.
 *
 * The class was inspired by the Qt example:
 * https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html */
#pragma once

#include <QPlainTextEdit>
#include <QPointer>
#include <QFileSystemWatcher>
#include <QDateTime>
#include <QString>
#include <QTimer>
#include "utils/SyntaxMode.h"

class CodeBlock;
class EditHistory;
class EditHistoryDialog;
class FileEncodingHandler;
class LineHistoryDialog;
class QPainter;
class STCSyntaxHighlighter;
class QNetworkAccessManager;

/** @brief Text editor widget with line numbers and support for STC markup.
 *
 * Extends QPlainTextEdit with:
 *  - loading and saving files (with encoding detection) and watching them for external changes,
 *  - tracking which lines differ from the originally loaded content,
 *  - detection of code blocks (e.g. `[cpp]...[/cpp]`) in the document,
 *  - whole-document syntax modes (STC, plain text, C++, Python, XML, JSON),
 *  - context-menu actions, smart pasting (links, tables, rich text) and spelling suggestions. */
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);
    ~CodeEditor();

    void newEmptyFile();

    SyntaxMode syntaxMode() const;
    /// Switches the highlighting of the whole document. Opening a file chooses the mode by its extension,
    /// this is what the "Syntax" menu calls to override it. `rehighlightNow=false` is for content which is about to be replaced.
    void setSyntaxMode(SyntaxMode mode, bool rehighlightNow = true);
    /// What the user chose in the "Syntax" menu: applied now and remembered for the current file,
    /// so it is the same after the file is opened again (`.txt` can be an STC article or a plain note).
    void setSyntaxModeChosenByUser(SyntaxMode mode);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    /// The whole text exactly as the user has it (see exactPlainText()): this is what is saved to the file and compared
    /// with it. toPlainText() would turn non-breaking spaces into plain ones.
    QString exactText() const;
    QStringList exactLines() const;
    /// Clicking a number which is circled shows the history of that line; the cursor and the tooltip say which are
    void lineNumberAreaMousePress(QMouseEvent *event);
    void lineNumberAreaMouseMove(QMouseEvent *event);

    /// Undo / redo: what Ctrl+Z, Ctrl+Shift+Z and the context menu call. They are QPlainTextEdit::undo() / redo()
    /// plus a word to the history of edits about what is going on.
    void undoWithHistory();
    void redoWithHistory();

    /// What is at the top edge of the viewport: the line (zero-based) and how far the viewport is inside it, 0..1
    /// (a long line is wrapped into many rows, so scrolling by rows moves inside one line)
    struct ViewportTop
    {
        int line = 0;
        double fraction = 0.0;
        bool atEndOfDocument = false; ///< scrolled all the way down (so the last lines are visible, whatever is at the top)
    };
    ViewportTop viewportTop() const;

    /// Table of the edits made to the document in this session (opened from the context menu)
    void showEditHistory();
    /// History of one line of the document (zero-based number), opened from the context menu or by a click on its number
    void showLineHistory(int line);

    bool noUnsavedChanges() const;

    const QString getFileName() const;
    void setFileName(const QString& newFileName);
    void enableWatchingOfFile(const QString& newFileName);

    void restoreStateWhichDoesNotRequireSaving(bool discardChanges=false);

    auto linesCount() const
    {
        return std::max<decltype(blockCount())>(1, blockCount());
    }

    bool loadFileContentDistargingCurrentContent(const QString& fileName);
    bool saveEntireContent2File(const QString& fileName);

    QMultiMap<QString, QKeySequence> listOfShortcuts() const;

    void markAsSaved();

    QString getFileModificationInfoText() const;

    const QVector<CodeBlock>& getCodeBlocks() const
    {
        return codeBlocks;
    }
    bool isInsideCode(int position) const;

    struct CodeBlockInfo // TODO: Do we need this if we have CodeBlock?
    {
        QString tag;
        int position;
    };
    std::optional<CodeBlockInfo> getCodeTagAtPosition(int position) const;

    std::optional<CodeBlock> selectEnclosingCodeBlock(int cursorPos);

    QTextCursor cursor4Line(int lineNumber) const;

    void reloadFromFile(bool discardChanges=false);

    const auto &getOriginalLines() const
    {
        return originalLines;
    }

    const QDateTime &getFileModificationTime() const
    {
        return fileModificationTime;
    }

    const QDateTime &getLastChangeTime() const
    {
        return lastChangeTime;
    }

    void setSearchHighlights(const QList<QTextEdit::ExtraSelection>& highlights);

    void stopWatchingFiles();

    // Backup-related functions
    void checkForBackupOnLoad();
    void createBackupTimer();
    void onBackupTimerTimeout();

    // Link-related functions
    void addLinkActionsIfApplicable(QMenu* menu);
    void copyLinkToClipboard();
    void removeLink();
    void selectLink();

signals:
    void shortcutPressed_bold();
    void shortcutPressed_run();
    void shortcutPressed_warning();
    void shortcutPressed_tip();
    void shortcutPressed_href();
    void shortcutPressed_h1();
    void shortcutPressed_h2();
    void shortcutPressed_h3();
    void shortcutPressed_h4();

    void totalLinesCountChanged(int currentLinesCount);

    void numberOfModifiedLinesChanged(int changedLinesCount);

    void codeBlocksChanged();

    void linkTitleFetchFailed(const QString& url, int lineNumber, const QString& reason);

    void contentReloaded();

    void syntaxModeChanged(SyntaxMode mode);

public slots:
    void fileChanged(const QString &path);

    void go2LineRequested(int lineNumber);
    void goToLineAndOffset(int lineNumber, int linePosition);

    void onScrollChanged(int);
    void onCursorPositionChanged();

    void analizeEntireDocumentDetectingCodeBlocks();

    void onContentsChange(int position, int charsRemoved, int charsAdded);

protected:
    void keyPressEvent(QKeyEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void contextMenuEvent(QContextMenuEvent* event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    void wheelEvent(QWheelEvent* event) override;

    void registerShortcuts();
    void connectSignalsWithSlots();

    void increaseFontSize();
    void decreaseFontSize();

    QString formatCppWithClang(const QString& code) const;

    bool isContentModified() const
    {
        return !modifiedLines.isEmpty();
    }

    auto modifiedLineCount() const
    {
        return modifiedLines.size();
    }

    void updateDiffWithOriginal();

    void trackOriginalVersionOfFile(const QString& fileName);

    QVector<CodeBlock> parseAllCodeBlocks();

    void handleCodeBlockDetectionOnChange(int position);

    /// 4th, 5th... click (STC mode): selects the value of the attribute / the text between the tags around the click,
    /// every next click selects the next, bigger region. Returns true if the event was handled.
    bool trySelectTagRegionOnMultiClick(QMouseEvent* event);
    int multiClickCount_ = 0;          ///< consecutive clicks of the left button: close in time and in place
    quint64 lastClickTimestamp_ = 0;   ///< [ms], timestamp of the event
    QPoint lastClickPosition_;

    /// methods to handle opening links on click:
    bool isCtrlLeftClick(QMouseEvent *event) const;
    bool tryOpenLinkAtPosition(const QString &text, int posInBlock);

    /// methods to load preview of images: both local and remote:
    std::optional<QString> extractImagePath(const QString& text, const QTextCursor &cursor) const;
    void showLocalImageTooltip(const QString &path, const QPoint &globalPos);
    void clearTooltipState();
    void showWebLinkPreview(const QString &url, const QPoint &globalPos);
    bool isLink(const QString &path) const;
    bool isLocalImageFile(const QString &path) const;

    /// methods to handle key pressed events:
    bool isControlOnly(QKeyEvent *event) const;
    void handleTabIndent();
    void handleTabUnindent();
    void applyToSelectedBlocks(const std::function<void (QTextCursor &)> &callback);
    bool handlePasteWithLinkWrapping();
    bool isCursorInsideImgSrcAttribute(const QTextCursor& cursor) const;
    bool isCursorInsideAHrefAttribute(const QTextCursor& cursor) const;
    bool isCursorInsideAttribute(const QTextCursor& cursor, const QString& tagName, const QString& attributeName) const;
    void fetchAndInsertTitle(const QString &url, int insertedPos);
    bool handlePasteTable();
    bool handlePastingRichText();

    /// methods to handle contest menu actions:
    void moveCursorToClickPosition(const QPoint &pos);
    void addCaseConversionActions(QMenu *menu, const QTextCursor &selection);
    void addWordFormatActions(QMenu *menu, const QTextCursor &selection);
    void addMultiLineSelectionActions(QMenu *menu, const QTextCursor &selection);
    bool selectionHasLineNumbering(const QTextCursor& selection) const;
    bool selectionHasBullets(const QTextCursor& selection) const;
    bool selectionHasBrokenNumbering(const QTextCursor& selection) const;
    void addTagRemovalActionIfInsideTag(QMenu *menu);
    void addCodeBlockActionsIfApplicable(QMenu *menu, const QPoint &pos);
    void addCppReferenceSearchActionIfApplicable(QMenu *menu, const QTextCursor& clickCursor);
    void addStcDocumentationActionIfApplicable(QMenu *menu, const QTextCursor& clickCursor);
    void addImgTagActionsIfApplicable(QMenu *menu);
    QString removeCppComments(const QString& code) const;
    QString removeExcessiveEmptyLines(const QString& code) const;
    void addPktTagActionsIfApplicable(QMenu *menu);
    void addCsvTagActionsIfApplicable(QMenu *menu);
    void addAnchorTagActionsIfApplicable(QMenu *menu);
    void addDivTagActionsIfApplicable(QMenu *menu);
    void addHeaderTagActionsIfApplicable(QMenu *menu, const QPoint &pos);
    void sortLinesInRange(int startLine, int endLine, bool ascending);
    // Checks if any selected line starts with a numbering pattern (e.g. '1. ')
    bool selectionHasLineNumbering() const;
    // Removes numbering from the left side of each selected line
    void removeLineNumberingFromSelection();
    // Renumbers lines in the selection that start with numbering, skipping lines without numbering
    void renumberSelection();
    std::optional<QPair<QString, QTextCursor>> getMisspelledWordAtPosition(const QPoint &pos);
    void addSpellingSuggestionsIfAvailable(QMenu* menu, const QPoint& pos);
    void editTableAtPosition(int csvStartPos, const QString& csvTag, int tagStart, int tagEnd);

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;

    EditHistory* editHistory = nullptr; // watches the document; child of this widget
    QPointer<EditHistoryDialog> editHistoryDialog;
    QPointer<LineHistoryDialog> lineHistoryDialog;
    /// Zero-based number of the line whose number is drawn at height `y` of the margin; -1 if none
    int lineAtGutterY(int y) const;
    void addEditHistoryActions(QMenu* menu, int clickedLine);
    /// Makes Undo and Redo of the standard menu go through the history; returns the Redo action (null if not found)
    QAction* routeUndoRedoThroughHistory(QMenu* menu);
    QList<QAction*> createEditHistoryActions(QMenu* menu, int clickedLine);

    // The margin with line numbers, one row at a time
    QFont lineNumberFont() const;
    void paintLineNumberRow(QPainter& painter, int blockNumber, int top) const;
    void paintModifiedLineBackground(QPainter& painter, int top) const;
    void paintCurrentLineArrow(QPainter& painter, int top) const;
    void paintLineNumber(QPainter& painter, int blockNumber, int top) const;
    void paintHistoryCircle(QPainter& painter, int blockNumber, int top) const;

    /// "Compile" in the menu of a code block: g++ for [cpp] and [code], syntax check and run for [py]
    void addCompileAction(QMenu* menu, const CodeBlock& codeOnlyBlock);
    QList<QTextEdit::ExtraSelection> persistentSearchHighlights;

    QFileSystemWatcher fileWatcher;
    QString lastTooltipImagePath; /// this variable is for image tool tips - to keep them visible longer

    QStringList originalLines;
    QSet<int> modifiedLines;
    QDateTime fileModificationTime;
    QDateTime lastChangeTime;

    QTimer* backupTimer;

    int currentLine = -1;

    QVector<CodeBlock> codeBlocks;

    QNetworkAccessManager* networkManager = {};

    std::unique_ptr<FileEncodingHandler> fileEncodingHandler;

    /// The mode of a file: what the user chose for it earlier, else what its extension says, else nothing.
    static std::optional<SyntaxMode> syntaxModeForFile(const QString& fileName);

    STCSyntaxHighlighter* syntaxHighlighter = nullptr; // owned by the document
    QString stcFontFamily; // font of the STC text, to get it back after leaving a source file mode
};
