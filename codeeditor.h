/// the code of the class was inspired by: https://doc.qt.io/qt-6.2/qtwidgets-widgets-codeeditor-example.html
#pragma once

#include <QPlainTextEdit>
#include <QFileSystemWatcher>
#include <QDateTime>

class CodeBlock;

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);
    ~CodeEditor();

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

signals:
    void totalLinesCountChanged(int currentLinesCount);
    void shortcutPressed_bold();
    void shortcutPressed_run();
    void shortcutPressed_warning();
    void shortcutPressed_tip();
    void shortcutPressed_href();
    void shortcutPressed_h1();
    void shortcutPressed_h2();
    void shortcutPressed_h3();
    void shortcutPressed_h4();

    void numberOfModifiedLinesChanged(int changedLinesCount);

    void codeBlocksChanged();

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

    void wheelEvent(QWheelEvent* event) override;

    void registerShortcuts();

    void increaseFontSize();
    void decreaseFontSize();

    QString formatCppWithClang(const QString& code);

    bool isContentModified() const
    {
        return !modifiedLines.isEmpty();
    }

    int modifiedLineCount() const
    {
        return modifiedLines.size();
    }

    void updateDiffWithOriginal();

    void trackOriginalVersionOfFile(const QString& fileName);

    QVector<CodeBlock> parseAllCodeBlocks();

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    QFileSystemWatcher fileWatcher;
    QString lastTooltipImagePath; /// this variable is for image tool tips - to keep them visible longer

    QStringList originalLines;
    QSet<int> modifiedLines;
    QDateTime fileModificationTime;
    QDateTime lastChangeTime;

    int currentLine = -1;

    QVector<CodeBlock> codeBlocks;
};
