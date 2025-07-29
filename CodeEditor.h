/// the code of the class was inspired by: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#pragma once

#include <QPlainTextEdit>
#include <QFileSystemWatcher>
#include <QDateTime>

class CodeBlock;
class FileEncodingHandler;
class QNetworkAccessManager;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);
    ~CodeEditor();

    void newEmptyFile();

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

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

    void wheelEvent(QWheelEvent* event) override;

    void registerShortcuts();
    void connectSignalsWithSlots();

    void increaseFontSize();
    void decreaseFontSize();

    QString formatCppWithClang(const QString& code);

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
    void fetchAndInsertTitle(const QString &url, int insertedPos);
    bool handlePasteTable();
    bool handlePastingRichText();

    /// methods to handle contest menu actions:
    void moveCursorToClickPosition(const QPoint &pos);
    void addCaseConversionActions(QMenu *menu, const QTextCursor &selection);
    void addWordFormatActions(QMenu *menu, const QTextCursor &selection);
    void addMultiLineSelectionActions(QMenu *menu, const QTextCursor &selection);
    void addTagRemovalActionIfInsideTag(QMenu *menu);
    void addCodeBlockActionsIfApplicable(QMenu *menu, const QPoint &pos);
    void addImgTagActionsIfApplicable(QMenu *menu);
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

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    QList<QTextEdit::ExtraSelection> persistentSearchHighlights;

    QFileSystemWatcher fileWatcher;
    QString lastTooltipImagePath; /// this variable is for image tool tips - to keep them visible longer

    QStringList originalLines;
    QSet<int> modifiedLines;
    QDateTime fileModificationTime;
    QDateTime lastChangeTime;

    int currentLine = -1;

    QVector<CodeBlock> codeBlocks;

    QNetworkAccessManager* networkManager = {};

    std::unique_ptr<FileEncodingHandler> fileEncodingHandler;
};
