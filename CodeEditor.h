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
#include <QAction>
#include <QFont>
#include <QKeySequence>
#include <QHash>
#include <QList>
#include <QMultiMap>
#include <QObject>
#include <QPair>
#include <QPoint>
#include <QSet>
#include <QStringList>
#include <QTextCursor>
#include <QTextEdit>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>

#include "utils/FoldRegions.h"
#include "utils/SyntaxMode.h"
#include "types/CodeBlock.h"

class EditHistory;
class EditHistoryDialog;
class FileEncodingHandler;
class LineHistoryDialog;
class LinkPreviewController;
class QPainter;
class STCSyntaxHighlighter;
class QNetworkAccessManager;
class QMenu;
class QRect;
class QTimer;
class QWidget;

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

    // ---- Folding: the section of a header or a block of code written in many lines collapses into one line ----
    // Works in the STC mode. Folding only hides the lines (the text is not changed, it is saved and compiled as it was),
    // so the cursor never stays in a hidden line: whatever puts it there (search, "go to line", a click in the preview,
    // typing at the end of a folded line) unfolds what hides it.

    /// What can be folded in the document now (STC mode): the sections of `[h1]`..`[h6]` and the blocks of code.
    /// Brought up to date a moment after the last change of the text (see updateFolding()).
    const QVector<stcfold::Region>& foldRegions() const { return foldRegions_; }
    /// Whether the line (zero-based) starts a region which can be folded
    bool isFoldable(int line) const { return foldRegionByStartLine_.contains(line); }
    /// Whether the region which starts in the line (zero-based) is folded now
    bool isFolded(int line) const;
    int foldedRegionsCount() const { return static_cast<int>(foldedRegions_.size()); }

    void foldLine(int line);
    void unfoldLine(int line);
    void toggleFoldAtLine(int line);

    /// The innermost region around the cursor (its first line, the lines inside and the closing tag of a block of code)
    /// which is not folded yet. Nothing happens when there is no such region.
    void foldAtCursor();
    /// Unfolds the region which the line of the cursor starts (only a folded line can be there)
    void unfoldAtCursor();
    /// Unfolds the region which starts in the line of the cursor, or else folds the innermost one around the cursor
    void toggleFoldAtCursor();
    void foldAll();
    void foldAllCodeBlocks();
    void unfoldAll();

    /// Brings the folds up to date with the text now. Normally it is done by itself a moment after the last change
    /// (for the regions, the markers and the hidden lines), the call is for the code which needs the result at once.
    void updateFolding();

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

    /// Puts the cursor at the position (a number of the character of the text) and makes it visible: the view is
    /// moved only when the position is not visible now, and then the position is put in the middle of it.
    void showPosition(int position);

    /// Table of the edits made to the document in this session (opened from the context menu)
    void showEditHistory();
    /// History of one line of the document (zero-based number), opened from the context menu or by a click on its number
    void showLineHistory(int line);

    bool noUnsavedChanges() const;

    const QString getFileName() const;
    void setFileName(const QString& newFileName);
    void enableWatchingOfFile(const QString& newFileName);

    void restoreStateWhichDoesNotRequireSaving(bool discardChanges=false);

    /// The user has decided to throw the unsaved changes away and the document is going to be closed or replaced
    /// anyway: only the automatic backup of the changes is removed (and no new one is made), the file is not read again.
    void discardUnsavedChanges();

    auto linesCount() const
    {
        return std::max<decltype(blockCount())>(1, blockCount());
    }

    bool loadFileContentDistargingCurrentContent(const QString& fileName);
    bool saveEntireContent2File(const QString& fileName);

    /// The encoding the file is saved in: the one it was read in, or the one chosen by the user (see setSaveEncoding())
    QString saveEncoding() const;
    /// Chooses the encoding the file is saved in. The text is kept; the file on disk gets the encoding when it is saved.
    void setSaveEncoding(const QString& encodingName);
    /// Reads the file again in the given encoding, when the detection was wrong. The unsaved changes are discarded:
    /// the caller has asked about them. Returns false if nothing was reloaded (the user declined).
    bool reloadFileWithEncoding(const QString& encodingName);

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

    /// Draws the "... N lines" labels at the end of folded lines
    void paintEvent(QPaintEvent *event) override;

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
    /// On a link: the switch of the preview of its page shown when the mouse rests on it
    void addLinkPreviewToggleAction(QMenu *menu, const QTextCursor& clickCursor);
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

    /// True while the whole content is being replaced (clear / load): the code blocks are analyzed once afterwards,
    /// not on every change reported by the document
    bool replacingWholeContent = false;

    LinkPreviewController* linkPreview = nullptr; // preview of the page of the link under the mouse; child of this widget
    EditHistory* editHistory = nullptr; // watches the document; child of this widget
    QPointer<EditHistoryDialog> editHistoryDialog;
    QPointer<LineHistoryDialog> lineHistoryDialog;
    /// Zero-based number of the line whose number is drawn at height `y` of the margin; -1 if none
    int lineAtGutterY(int y) const;
    void addEditHistoryActions(QMenu* menu, int clickedLine);
    /// Makes Undo and Redo of the standard menu go through the history; returns the Redo action (null if not found)
    QAction* routeUndoRedoThroughHistory(QMenu* menu);
    QList<QAction*> createEditHistoryActions(QMenu* menu, int clickedLine);

    // Folding (CodeEditorFolding.cpp)
    struct FoldedRegion
    {
        QTextCursor anchor;       ///< at the start of the first line of the region: follows the line when the text changes
        stcfold::Kind kind;
        int level;
    };
    void setupFolding();
    void scheduleFoldingUpdate();
    bool foldingAvailable() const;
    /// Width of the column of the margin with the markers of the regions: 0 when folding is not available
    int foldColumnWidth() const;
    /// Right edge of the part of the margin with the line numbers (the column of the markers is on its right)
    int lineNumbersRightEdge() const;
    void foldRegionsByIndex(const QVector<int>& indices);
    void dropFoldsHidingLine(int line);
    /// Makes the lines hidden exactly when a folded region hides them, then updates the view
    void applyFolding();
    void paintFoldMarker(QPainter& painter, int blockNumber, int top) const;
    /// The first line of the folded region whose "... N lines" label is under the point (coordinates of the viewport), -1 if none
    int foldLabelLineAt(const QPoint& viewportPoint) const;
    void addFoldingActions(QMenu* menu);
    int innermostFoldableRegionAround(int line) const;

    QVector<stcfold::Region> foldRegions_;
    QHash<int, int> foldRegionByStartLine_; ///< the first line of a region -> its index in foldRegions_
    QVector<FoldedRegion> foldedRegions_;
    QSet<int> foldedStartLines_;            ///< first lines of foldedRegions_, for the markers (refreshed with the folds)
    QTimer* foldingTimer_ = nullptr;
    QVector<QPair<QRect, int>> foldLabels_; ///< where the "... N lines" labels were drawn (viewport coordinates) and the first line of their regions
    int lastFoldColumnWidth_ = 0;
    bool overFoldLabel_ = false;

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
