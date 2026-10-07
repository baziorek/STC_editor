#include "cppcompilerdialog.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

#include "CodeEditor.h"
#include "utils/SyntaxMode.h"

namespace
{
constexpr int kCompileTimeoutMs = 120'000;
constexpr int kRunTimeoutMs = 10'000;
constexpr qint64 kMaxOutputBytes = 1 << 20; // a program printing in an endless loop must not eat all the memory

constexpr char kSettingCompileFlags[] = "cppCompiler/compileFlags";
constexpr char kSettingLinkFlags[] = "cppCompiler/linkFlags";
constexpr char kSettingRunAfterCompile[] = "cppCompiler/runAfterCompile";
constexpr char kDefaultCompileFlags[] = "-std=c++23";

constexpr char kSourceFileName[] = "main.cpp"; // relative name: the compiler messages say "main.cpp:5:3: error", not "/tmp/.../codeX.cpp:5:3"
#ifdef Q_OS_WIN
constexpr char kProgramFileName[] = "program.exe";
#else
constexpr char kProgramFileName[] = "program";
#endif

QString normalizedNewlines(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    text.replace(QChar::LineSeparator, QLatin1Char('\n'));
    return text;
}

/// Splits the text of a flags field into arguments the way a shell does: whitespace separates the arguments,
/// '...' is literal, "..." allows \" and \\ and a backslash escapes the next character, so `-DNAME='"a b"'` works.
/// On Windows a backslash is a part of a path (`-IC:\libs\include`), there QProcess::splitCommand() is used.
QStringList splitFlags(const QString& text)
{
#ifdef Q_OS_WIN
    return QProcess::splitCommand(text);
#else
    QStringList arguments;
    QString current;
    bool inArgument = false;
    QChar quote; // null when outside of quotes

    for (qsizetype i = 0; i < text.size(); ++i)
    {
        const QChar ch = text[i];
        if (quote == QLatin1Char('\''))
        {
            if (ch == quote)
                quote = QChar();
            else
                current += ch;
        }
        else if (quote == QLatin1Char('"'))
        {
            if (ch == quote)
                quote = QChar();
            else if (ch == QLatin1Char('\\') && i + 1 < text.size() && (text[i + 1] == QLatin1Char('"') || text[i + 1] == QLatin1Char('\\')))
                current += text[++i];
            else
                current += ch;
        }
        else if (ch == QLatin1Char('\'') || ch == QLatin1Char('"'))
        {
            quote = ch;
            inArgument = true;
        }
        else if (ch == QLatin1Char('\\') && i + 1 < text.size())
        {
            current += text[++i];
            inArgument = true;
        }
        else if (ch.isSpace())
        {
            if (inArgument)
                arguments << current;
            current.clear();
            inArgument = false;
        }
        else
        {
            current += ch;
            inArgument = true;
        }
    }
    if (inArgument)
        arguments << current;
    return arguments;
#endif
}
} // namespace

CppCompilerDialog::CppCompilerDialog(const CodeBlock& block, QWidget* parent)
    : QDialog(parent), articleCodeCursor_(block.cursor), tag_(block.tag)
{
    setWindowTitle(tr("g++ compilation"));
    resize(1000, 780);

    timeoutTimer_ = new QTimer(this);
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, &CppCompilerDialog::onTimeout);

    buildUi();
    loadSettings();

    codeEdit_->setPlainText(normalizedNewlines(block.cursor.selectedText()));
    codeEdit_->markAsSaved(); // otherwise every line would be marked in the gutter as modified
    codeEdit_->moveCursor(QTextCursor::Start);

    setStage(Stage::Idle);
    updateInsertButtons();

    // Same as before: the dialog compiles the code as soon as it is opened
    QMetaObject::invokeMethod(this, &CppCompilerDialog::startCompilation, Qt::QueuedConnection);
}

CppCompilerDialog::~CppCompilerDialog()
{
    saveSettings();
    releaseProcess();
}

void CppCompilerDialog::buildUi()
{
    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    // --- code: the same editor (C++ highlighting, line numbers) as in the main window
    codeEdit_ = new CodeEditor(this);
    codeEdit_->setSyntaxMode(SyntaxMode::Cpp);

    // --- flags
    compileFlagsEdit_ = new QLineEdit(this);
    compileFlagsEdit_->setPlaceholderText(tr("e.g. -std=c++23 -O2 -Wall -Wextra"));
    compileFlagsEdit_->setClearButtonEnabled(true);
    compileFlagsEdit_->setToolTip(tr("Passed to g++ before the source file. Split like in a shell: '...', \"...\" and \\ work, e.g. -DNAME='\"a b\"'"));

    linkFlagsEdit_ = new QLineEdit(this);
    linkFlagsEdit_->setPlaceholderText(tr("e.g. -lpthread -lm  or  -L/path/to/libs -lfoo"));
    linkFlagsEdit_->setClearButtonEnabled(true);
    linkFlagsEdit_->setToolTip(tr("Passed to g++ after the source file (libraries, -L paths, ...)"));

    runAfterCompileCheck_ = new QCheckBox(tr("Run the program after a successful compilation"), this);

    runButton_ = new QPushButton(tr("Compile (F5)"), this);
    runButton_->setShortcut(Qt::Key_F5);
    runButton_->setDefault(true);
    stopButton_ = new QPushButton(tr("Stop"), this);
    stopButton_->setAutoDefault(false);

    statusLabel_ = new QLabel(this);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* flagsLayout = new QGridLayout;
    flagsLayout->addWidget(new QLabel(tr("Compiler flags:"), this), 0, 0);
    flagsLayout->addWidget(compileFlagsEdit_, 0, 1);
    flagsLayout->addWidget(runButton_, 0, 2);
    flagsLayout->addWidget(new QLabel(tr("Linker flags / libraries:"), this), 1, 0);
    flagsLayout->addWidget(linkFlagsEdit_, 1, 1);
    flagsLayout->addWidget(stopButton_, 1, 2);
    flagsLayout->setColumnStretch(1, 1);

    auto* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(runAfterCompileCheck_);
    statusLayout->addStretch(1);
    statusLayout->addWidget(statusLabel_);

    // --- logs
    auto makeLogEdit = [&](const QString& placeholder) {
        auto* edit = new QPlainTextEdit(this);
        edit->setFont(monoFont);
        edit->setLineWrapMode(QPlainTextEdit::NoWrap);
        edit->setPlaceholderText(placeholder);
        connect(edit, &QPlainTextEdit::textChanged, this, &CppCompilerDialog::updateInsertButtons);
        return edit;
    };
    compilerLogEdit_ = makeLogEdit(tr("Compiler messages (errors, warnings)"));
    programOutputEdit_ = makeLogEdit(tr("Output of the program"));

    insertCompilerLogButton_ = new QPushButton(tr("Insert compiler log below the code"), this);
    insertCompilerLogButton_->setToolTip(tr("Adds [code]...[/code] with this text under the closing tag of the code block"));
    insertCompilerLogButton_->setAutoDefault(false);
    insertProgramOutputButton_ = new QPushButton(tr("Insert program output below the code"), this);
    insertProgramOutputButton_->setToolTip(tr("Adds [code]...[/code] with this text under the closing tag of the code block"));
    insertProgramOutputButton_->setAutoDefault(false);

    auto makePane = [&](const QString& title, QPlainTextEdit* edit, QPushButton* button) {
        auto* pane = new QWidget(this);
        auto* layout = new QVBoxLayout(pane);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(QStringLiteral("<b>%1</b>").arg(title), pane);
        layout->addWidget(label);
        layout->addWidget(edit, 1);
        layout->addWidget(button);
        return pane;
    };

    auto* logsSplitter = new QSplitter(Qt::Horizontal, this);
    logsSplitter->addWidget(makePane(tr("Compiler log"), compilerLogEdit_, insertCompilerLogButton_));
    logsSplitter->addWidget(makePane(tr("Program output"), programOutputEdit_, insertProgramOutputButton_));

    auto* codeAndLogsSplitter = new QSplitter(Qt::Vertical, this);
    codeAndLogsSplitter->addWidget(codeEdit_);
    codeAndLogsSplitter->addWidget(logsSplitter);
    codeAndLogsSplitter->setStretchFactor(0, 3);
    codeAndLogsSplitter->setStretchFactor(1, 2);

    // --- bottom row
    replaceCodeButton_ = new QPushButton(tr("Put the code back into the article"), this);
    replaceCodeButton_->setToolTip(tr("Replaces the content of the code block in the article with the code from this window"));
    replaceCodeButton_->setAutoDefault(false);
    auto* closeButton = new QPushButton(tr("Close"), this);
    closeButton->setAutoDefault(false);

    auto* bottomLayout = new QHBoxLayout;
    bottomLayout->addWidget(replaceCodeButton_);
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(closeButton);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(flagsLayout);
    mainLayout->addLayout(statusLayout);
    mainLayout->addWidget(codeAndLogsSplitter, 1);
    mainLayout->addLayout(bottomLayout);

    connect(runButton_, &QPushButton::clicked, this, &CppCompilerDialog::startCompilation);
    connect(stopButton_, &QPushButton::clicked, this, &CppCompilerDialog::stopRunning);
    connect(insertCompilerLogButton_, &QPushButton::clicked, this, &CppCompilerDialog::insertCompilerLogBelowCode);
    connect(insertProgramOutputButton_, &QPushButton::clicked, this, &CppCompilerDialog::insertProgramOutputBelowCode);
    connect(replaceCodeButton_, &QPushButton::clicked, this, &CppCompilerDialog::replaceCodeInArticle);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void CppCompilerDialog::loadSettings()
{
    QSettings settings;
    compileFlagsEdit_->setText(settings.value(kSettingCompileFlags, QString::fromLatin1(kDefaultCompileFlags)).toString());
    linkFlagsEdit_->setText(settings.value(kSettingLinkFlags).toString());
    runAfterCompileCheck_->setChecked(settings.value(kSettingRunAfterCompile, true).toBool());
}

void CppCompilerDialog::saveSettings() const
{
    QSettings settings;
    settings.setValue(kSettingCompileFlags, compileFlagsEdit_->text());
    settings.setValue(kSettingLinkFlags, linkFlagsEdit_->text());
    settings.setValue(kSettingRunAfterCompile, runAfterCompileCheck_->isChecked());
}

// ------------------------------------------------------------------ running

void CppCompilerDialog::startCompilation()
{
    if (stage_ != Stage::Idle)
        return;

    saveSettings();
    compilerLogEdit_->clear();
    programOutputEdit_->clear();

    workDir_ = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/stc_cppXXXXXX"));
    if (!workDir_->isValid())
    {
        setStatus(tr("Failed to create a temporary directory."), true);
        return;
    }

    QFile source(workDir_->filePath(QString::fromLatin1(kSourceFileName)));
    if (!source.open(QIODevice::WriteOnly))
    {
        setStatus(tr("Failed to create the temporary source file."), true);
        return;
    }
    source.write(codeEdit_->toPlainText().toUtf8());
    source.close();

    QStringList arguments;
    arguments << QStringLiteral("-fdiagnostics-color=never"); // no escape sequences in the log which is pasted into the article
    arguments += splitFlags(compileFlagsEdit_->text());
    arguments << QString::fromLatin1(kSourceFileName);
    arguments += splitFlags(linkFlagsEdit_->text()); // libraries have to follow the source file
    arguments << QStringLiteral("-o") << QString::fromLatin1(kProgramFileName);

    setStage(Stage::Compiling);
    setStatus(tr("Compiling..."));
    statusLabel_->setToolTip(QStringLiteral("g++ ") + arguments.join(QLatin1Char(' ')));
    startProcess(QStringLiteral("g++"), arguments, kCompileTimeoutMs);
}

void CppCompilerDialog::runProgram()
{
    setStage(Stage::Running);
    setStatus(tr("Running the program..."));
    statusLabel_->setToolTip(QString());
    startProcess(workDir_->filePath(QString::fromLatin1(kProgramFileName)), {}, kRunTimeoutMs);
}

void CppCompilerDialog::startProcess(const QString& program, const QStringList& arguments, int timeoutMs)
{
    releaseProcess();

    receivedBytes_ = 0;
    stoppedByUser_ = false;
    timedOut_ = false;
    outputTruncated_ = false;
    decoder_ = QStringDecoder(QStringDecoder::Utf8);

    process_ = new QProcess(this);
    process_->setWorkingDirectory(workDir_->path());
    process_->setProcessChannelMode(QProcess::MergedChannels); // stdout and stderr in the order in which they were written
    process_->setStandardInputFile(QProcess::nullDevice());    // a program waiting for std::cin gets EOF instead of hanging

    connect(process_, &QProcess::readyReadStandardOutput, this, &CppCompilerDialog::onProcessOutput);
    connect(process_, &QProcess::finished, this, &CppCompilerDialog::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CppCompilerDialog::onProcessError);

    timeoutTimer_->start(timeoutMs);
    process_->start(program, arguments);
}

void CppCompilerDialog::releaseProcess()
{
    timeoutTimer_->stop();
    if (!process_)
        return;

    process_->disconnect(this);
    if (process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(1000);
    }
    process_->deleteLater();
    process_ = nullptr;
}

void CppCompilerDialog::stopRunning()
{
    if (!process_)
        return;
    stoppedByUser_ = true;
    process_->kill();
}

void CppCompilerDialog::onTimeout()
{
    if (!process_)
        return;
    timedOut_ = true;
    process_->kill();
}

QPlainTextEdit* CppCompilerDialog::currentPane() const
{
    return stage_ == Stage::Running ? programOutputEdit_ : compilerLogEdit_;
}

void CppCompilerDialog::onProcessOutput()
{
    if (!process_)
        return;

    const QByteArray data = process_->readAllStandardOutput();
    if (data.isEmpty() || outputTruncated_)
        return;

    receivedBytes_ += data.size();
    QString text = decoder_(data);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    QPlainTextEdit* pane = currentPane();
    pane->moveCursor(QTextCursor::End);
    pane->insertPlainText(text);
    pane->verticalScrollBar()->setValue(pane->verticalScrollBar()->maximum());

    if (receivedBytes_ > kMaxOutputBytes)
    {
        outputTruncated_ = true;
        process_->kill();
    }
}

void CppCompilerDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    onProcessOutput(); // whatever is still in the pipe

    const Stage finishedStage = stage_;
    const QString what = finishedStage == Stage::Compiling ? tr("Compilation") : tr("The program");
    releaseProcess();

    if (stoppedByUser_)
    {
        setStatus(tr("%1 was stopped.").arg(what), true);
    }
    else if (timedOut_)
    {
        const int seconds = (finishedStage == Stage::Compiling ? kCompileTimeoutMs : kRunTimeoutMs) / 1000;
        setStatus(tr("%1 took longer than %2 s and was killed.").arg(what).arg(seconds), true);
    }
    else if (outputTruncated_)
    {
        setStatus(tr("%1 printed more than 1 MiB, it was killed and the output is truncated.").arg(what), true);
    }
    else if (exitStatus == QProcess::CrashExit)
    {
        setStatus(tr("%1 crashed.").arg(what), true);
    }
    else if (finishedStage == Stage::Compiling)
    {
        if (exitCode != 0)
        {
            setStatus(tr("Compilation failed (exit code %1).").arg(exitCode), true);
        }
        else if (runAfterCompileCheck_->isChecked())
        {
            runProgram();
            return; // the stage stays "running"
        }
        else
        {
            setStatus(tr("Compilation succeeded."));
        }
    }
    else
    {
        setStatus(tr("The program finished, exit code %1.").arg(exitCode), exitCode != 0);
    }

    setStage(Stage::Idle);
}

void CppCompilerDialog::onProcessError(QProcess::ProcessError error)
{
    if (error != QProcess::FailedToStart) // the other errors are followed by finished()
        return;

    const bool compiling = stage_ == Stage::Compiling;
    releaseProcess();
    setStatus(compiling ? tr("Failed to start g++. Is it installed and available in PATH?")
                        : tr("Failed to start the compiled program."),
              true);
    setStage(Stage::Idle);
}

void CppCompilerDialog::setStage(Stage stage)
{
    stage_ = stage;
    runButton_->setEnabled(stage == Stage::Idle);
    stopButton_->setEnabled(stage != Stage::Idle);
}

void CppCompilerDialog::setStatus(const QString& text, bool isError)
{
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(isError ? QStringLiteral("color: #c62828; font-weight: bold;") : QString());
}

// ------------------------------------------------------------------ the article

void CppCompilerDialog::updateInsertButtons()
{
    insertCompilerLogButton_->setEnabled(!compilerLogEdit_->toPlainText().trimmed().isEmpty());
    insertProgramOutputButton_->setEnabled(!programOutputEdit_->toPlainText().trimmed().isEmpty());
}

void CppCompilerDialog::replaceCodeInArticle()
{
    const QString newCode = codeEdit_->toPlainText();

    QTextCursor edit = articleCodeCursor_;
    const int start = edit.selectionStart();
    edit.beginEditBlock(); // one step of undo in the main editor
    edit.insertText(newCode);
    edit.endEditBlock();

    // The code block now spans the new text, so the buttons below keep working
    articleCodeCursor_.setPosition(start);
    articleCodeCursor_.setPosition(start + newCode.length(), QTextCursor::KeepAnchor);

    codeEdit_->markAsSaved(); // the gutter marks the lines changed since the last replace
    setStatus(tr("The code in the article has been replaced."));
}

int CppCompilerDialog::endOfClosingTagLine() const
{
    QTextDocument* document = articleCodeCursor_.document();
    const int codeEnd = articleCodeCursor_.selectionEnd();
    const QString closingTag = QStringLiteral("[/%1]").arg(tag_);

    QTextCursor probe(document);
    probe.setPosition(codeEnd);
    probe.setPosition(qMin(codeEnd + static_cast<int>(closingTag.size()), document->characterCount() - 1), QTextCursor::KeepAnchor);
    const int afterTag = probe.selectedText() == closingTag ? probe.position() : codeEnd;

    const QTextBlock block = document->findBlock(afterTag);
    return block.position() + block.length() - 1; // before the line break
}

void CppCompilerDialog::insertCodeBlockBelow(const QString& text)
{
    QString body = normalizedNewlines(text);
    while (body.endsWith(QLatin1Char('\n')))
        body.chop(1);
    if (body.trimmed().isEmpty())
        return;

    QTextCursor cursor(articleCodeCursor_.document());
    cursor.setPosition(endOfClosingTagLine());
    cursor.beginEditBlock(); // one step of undo in the main editor
    cursor.insertText(QStringLiteral("\n[code]") + body + QStringLiteral("[/code]"));
    cursor.endEditBlock();
}

void CppCompilerDialog::insertCompilerLogBelowCode()
{
    insertCodeBlockBelow(compilerLogEdit_->toPlainText());
    setStatus(tr("The compiler log has been inserted below the code."));
}

void CppCompilerDialog::insertProgramOutputBelowCode()
{
    insertCodeBlockBelow(programOutputEdit_->toPlainText());
    setStatus(tr("The program output has been inserted below the code."));
}
