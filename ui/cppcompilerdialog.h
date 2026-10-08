#pragma once

#include <memory>
#include <vector>
#include <QElapsedTimer>
#include <QDialog>
#include <QProcess>
#include <QString>
#include <QStringConverter>
#include <QTextCursor>
#include "types/CodeBlock.h"
#include "utils/AnsiTextWriter.h"

class CodeEditor;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTemporaryDir;
class QTimer;

/// Compiles (and optionally runs) the C++ code of an STC `[cpp]` block.
///
/// The dialog shows:
///  - the code in an editor with the same C++ highlighting and line numbers as the main window
///    (so the line numbers of the compiler messages match the gutter),
///  - fields with the compiler flags and the linker flags/libraries,
///  - the compiler log and the output of the program in separate panes,
///  - buttons which insert the log / the output below the `[cpp]...[/cpp]` block as `[code]...[/code]`,
///    and a button which puts the (possibly edited) code back into the article.
class CppCompilerDialog : public QDialog
{
    Q_OBJECT

public:
    /// `block.cursor` has to select only the code (between `[cpp]` and `[/cpp]`) in the document of the main editor,
    /// that is what `CodeEditor::selectEnclosingCodeBlock()` returns.
    explicit CppCompilerDialog(const CodeBlock& block, QWidget* parent = nullptr);
    ~CppCompilerDialog() override;

private slots:
    void startCompilation();
    void stopRunning();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();
    void onProcessErrorOutput();
    void rebuildProgramOutput();
    void onTimeout();

    void replaceCodeInArticle();
    void insertCompilerLogBelowCode();
    void insertProgramOutputBelowCode();

private:
    enum class Stage { Idle, Compiling, Running };

    void buildUi();
    void loadSettings();
    void saveSettings() const;

    void startProcess(const QString& program, const QStringList& arguments, int timeoutMs);
    void releaseProcess();
    void drainOutput();
    void handleOutput(const QByteArray& data, bool isStderr);
    void appendProgramText(bool isStderr, const QString& text);
    bool isStreamVisible(bool isStderr) const;
    void runProgram();
    void setStage(Stage stage);
    void setStatus(const QString& text, bool isError = false);
    QPlainTextEdit* currentPane() const;
    void updateInsertButtons();

    /// Inserts `[code]\n<text>\n[/code]` in a new line below the closing tag of the code block.
    void insertCodeBlockBelow(const QString& text);
    /// Where the line with the closing `[/cpp]` ends in the document of the main editor.
    int endOfClosingTagLine() const;

    CodeEditor* codeEdit_ = nullptr;
    QLineEdit* compileFlagsEdit_ = nullptr;
    QLineEdit* linkFlagsEdit_ = nullptr;
    QCheckBox* runAfterCompileCheck_ = nullptr;
    QPlainTextEdit* compilerLogEdit_ = nullptr;
    QPlainTextEdit* programOutputEdit_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* insertCompilerLogButton_ = nullptr;
    QPushButton* insertProgramOutputButton_ = nullptr;
    QPushButton* replaceCodeButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* compileTimeLabel_ = nullptr;
    QLabel* runTimeLabel_ = nullptr;
    QCheckBox* showStdoutCheck_ = nullptr;
    QCheckBox* showStderrCheck_ = nullptr;

    QTextCursor articleCodeCursor_; ///< selects the code in the main editor (follows its edits)
    QString tag_;                   ///< "cpp"

    Stage stage_ = Stage::Idle;
    QProcess* process_ = nullptr;
    QTimer* timeoutTimer_ = nullptr;
    std::unique_ptr<QTemporaryDir> workDir_;
    QStringDecoder decoder_{QStringDecoder::Utf8};       ///< stdout (and stderr while compiling: the channels are merged)
    QStringDecoder stderrDecoder_{QStringDecoder::Utf8}; ///< stderr of the running program
    AnsiTextWriter ansiWriter_;       ///< shows the colors of the compiler messages, keeps the text plain
    AnsiTextWriter stdoutWriter_;     ///< the same for the stdout of the program
    AnsiTextWriter stderrWriter_;     ///< the same for the stderr of the program, which is red by default

    /// What the program has written so far, in the order of arrival, so that the stdout / stderr checkboxes can refilter it.
    struct OutputChunk
    {
        bool isStderr;
        QString text;
    };
    std::vector<OutputChunk> programChunks_;
    QElapsedTimer processTimer_;
    qint64 receivedBytes_ = 0;
    bool stoppedByUser_ = false;
    bool timedOut_ = false;
    bool outputTruncated_ = false;
};
