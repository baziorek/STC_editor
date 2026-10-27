#include "ConsoleWidget.h"

#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTimer>
#include <QProcess>
#include <QRgb>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <Qt>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "utils/ConsoleShell.h"
#include "widgets/ConsoleInputEdit.h"

namespace
{
constexpr int kMaxOutputLines = 10000;
constexpr int kMaxClassifiedLines = 20000; ///< `find /` should not freeze the window
constexpr int kDurationRefreshMs = 200;

/// Stops the command and everything which it started. On Unix the command is the leader of its own process group
/// (see `startProcess()`), so one signal reaches the whole pipeline (`make` and the compilers below it).
void signalProcessTree(QProcess* process, bool force)
{
#ifdef Q_OS_UNIX
    const pid_t pid = static_cast<pid_t>(process->processId());
    if (pid > 0)
    {
        const int signal = force ? SIGKILL : SIGINT;
        // the group exists once the child called setsid(); a click in the first milliseconds finds only the process
        if (::kill(-pid, signal) != 0)
            ::kill(pid, signal);
    }
#else
    Q_UNUSED(force);
    // there is no Ctrl+C for a process without a console; taskkill ends the whole tree
    if (process->processId() != 0)
        QProcess::startDetached(QStringLiteral("taskkill"),
                                {QStringLiteral("/PID"), QString::number(process->processId()), QStringLiteral("/T"), QStringLiteral("/F")});
#endif
}
} // namespace

ConsoleWidget::ConsoleWidget(QWidget* parent) : QWidget(parent), workingDirectory_(QDir::homePath())
{
    const QFont monospace = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    output_ = new ConsoleOutputEdit(this);
    output_->setDefaultDirectory(workingDirectory_);
    output_->setMaximumBlockCount(kMaxOutputLines);
    output_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    output_->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    output_->setFont(monospace);

    prompt_ = new QLabel(this);
    prompt_->setFont(monospace);
    prompt_->setTextInteractionFlags(Qt::NoTextInteraction);

    input_ = new ConsoleInputEdit(this);
    input_->setFont(monospace);
    input_->setWorkingDirectory(workingDirectory_);

    stopButton_ = new QToolButton(this);
    stopButton_->setText(QStringLiteral("■"));
    stopButton_->setAutoRaise(true);
    stopButton_->hide();

    clearButton_ = new QToolButton(this);
    clearButton_->setAutoRaise(true);
    clearButton_->setText(QStringLiteral("✕"));

    commandTick_ = new QTimer(this);
    commandTick_->setInterval(kDurationRefreshMs);

    auto* inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(4, 0, 4, 2);
    inputRow->addWidget(prompt_);
    inputRow->addWidget(input_, /*stretch=*/1);
    inputRow->addWidget(stopButton_);
    inputRow->addWidget(clearButton_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(output_, /*stretch=*/1);
    layout->addLayout(inputRow);

    connect(input_, &ConsoleInputEdit::commandSubmitted, this, &ConsoleWidget::submit);
    connect(input_, &ConsoleInputEdit::interruptRequested, this, [this] {
        if (process_)
            interruptCommand();
        else
            input_->clear(); // like ^C in a shell: the line is dropped
    });
    connect(input_, &ConsoleInputEdit::clearOutputRequested, this, &ConsoleWidget::clearOutput);
    connect(input_, &ConsoleInputEdit::endOfInputRequested, this, [this] {
        if (process_)
            process_->closeWriteChannel();
    });
    connect(input_, &ConsoleInputEdit::leaveRequested, this, &ConsoleWidget::leaveRequested);
    connect(stopButton_, &QToolButton::clicked, this, &ConsoleWidget::interruptCommand);
    connect(clearButton_, &QToolButton::clicked, this, &ConsoleWidget::clearOutput);
    connect(output_, &ConsoleOutputEdit::clearRequested, this, &ConsoleWidget::clearOutput);
    connect(output_, &ConsoleOutputEdit::pathActivated, this, [this](const QString& path) {
        const QFileInfo info(path);
        emit openDirectoryRequested(info.isDir() ? path : info.absolutePath());
    });
    connect(commandTick_, &QTimer::timeout, this, [this] {
        if (currentCommand_)
        {
            currentCommand_->durationMs = commandTimer_.elapsed();
            output_->refreshMargin();
        }
    });

    setFocusProxy(input_);
    updatePrompt();
    retranslate();
}

ConsoleWidget::~ConsoleWidget()
{
    if (process_)
    {
        disconnect(process_, nullptr, this, nullptr); // nobody listens any more
        signalProcessTree(process_, /*force=*/true);
        process_->waitForFinished(1000);
    }
}

void ConsoleWidget::setWorkingDirectory(const QString& directory)
{
    const QFileInfo info(directory);
    if (directory.isEmpty() || !info.isDir())
        return;

    const QString cleaned = QDir::cleanPath(info.absoluteFilePath());
    if (cleaned == workingDirectory_)
        return;

    previousDirectory_ = workingDirectory_;
    workingDirectory_ = cleaned;
    output_->setDefaultDirectory(cleaned);
    input_->setWorkingDirectory(cleaned);
    updatePrompt();
    emit workingDirectoryChanged(cleaned);
}

void ConsoleWidget::setCurrentFileProvider(std::function<QString()> provider)
{
    currentFile_ = std::move(provider);
}

void ConsoleWidget::setHistory(const QStringList& commands)
{
    input_->setHistory(commands);
}

QStringList ConsoleWidget::history() const
{
    return input_->history();
}

QString ConsoleWidget::outputText() const
{
    return output_->toPlainText();
}

void ConsoleWidget::clearOutput()
{
    output_->clear();
}

void ConsoleWidget::focusInput()
{
    input_->setFocus();
}

void ConsoleWidget::runCommand(const QString& line)
{
    submit(line);
}

void ConsoleWidget::interruptCommand()
{
    if (!process_)
        return;
    const bool force = interruptCount_ > 0; // it did not stop after the first time
    ++interruptCount_;
    signalProcessTree(process_, force);
}

// ------------------------------------------------------------------ running

void ConsoleWidget::submit(const QString& rawLine)
{
    if (process_)
    {
        // a command is running: the text is for the program (there is no terminal, which would show it)
        echoStandardInput(rawLine);
        process_->write(rawLine.toLocal8Bit() + '\n');
        return;
    }

    const QString line = rawLine.trimmed();
    if (line.isEmpty())
        return;

    if (input_->addToHistory(line))
        emit historyChanged(input_->history());

    const ConsoleShell::Builtin builtin = ConsoleShell::parseBuiltin(line, processEnvironment());
    if (builtin.kind == ConsoleShell::BuiltinKind::Clear)
    {
        clearOutput();
        return;
    }

    const bool isBuiltin = builtin.kind == ConsoleShell::BuiltinKind::ChangeDirectory;
    const std::shared_ptr<ConsoleCommandInfo> info = echoCommand(line, /*showStats=*/!isBuiltin);
    if (isBuiltin)
        changeDirectory(builtin.argument);
    else
        startProcess(line, info);
}

void ConsoleWidget::changeDirectory(const QString& argument)
{
    const QString target = ConsoleShell::resolveCdTarget(workingDirectory_, argument, QDir::homePath(), previousDirectory_);
    if (target.isEmpty())
    {
        appendStatus(tr("cd: there is no previous directory"), /*error=*/true);
        return;
    }
    if (!QFileInfo(target).isDir())
    {
        appendStatus(tr("cd: %1: no such directory").arg(argument), /*error=*/true);
        return;
    }
    setWorkingDirectory(target);
}

QProcessEnvironment ConsoleWidget::processEnvironment() const
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // The output goes to a window, not to a terminal, so programs which color their output only on a terminal
    // are asked to do it anyway (cmake, ls on BSD/macOS, node, ...); the colors are shown by AnsiTextWriter.
    environment.insert(QStringLiteral("CLICOLOR_FORCE"), QStringLiteral("1"));
    environment.insert(QStringLiteral("FORCE_COLOR"), QStringLiteral("1"));
    environment.insert(QStringLiteral("CMAKE_COLOR_DIAGNOSTICS"), QStringLiteral("ON"));
    environment.insert(QStringLiteral("PWD"), workingDirectory_);

    const QString file = currentFile_ ? currentFile_() : QString();
    if (!file.isEmpty())
    {
        const QFileInfo info(file);
        environment.insert(QStringLiteral("STC_FILE"), info.absoluteFilePath());
        environment.insert(QStringLiteral("STC_DIR"), info.absolutePath());
    }
    return environment;
}

void ConsoleWidget::startProcess(const QString& line, const std::shared_ptr<ConsoleCommandInfo>& info)
{
    writer_.reset();
    decoder_ = QStringDecoder(QStringConverter::System);
    pendingCarriageReturn_ = false;
    interruptCount_ = 0;

    currentCommand_ = info;
    currentCommand_->running = true;
    currentCommand_->lineCount = 0;
    currentCommand_->durationMs = 0;
    commandTimer_.start();
    commandTick_->start();

    colorListing_ = ConsoleShell::isListingCommand(line);
    if (colorListing_)
    {
        listing_ = ConsoleShell::ListingClassifier(workingDirectory_);
        listingCursor_ = QTextCursor(output_->document());
        listingCursor_.movePosition(QTextCursor::End); // the echo of the command ends with a line break
        listingCursor_.setKeepPositionOnInsert(true);
        classifiedLines_ = 0;
    }

    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    process_->setProcessEnvironment(processEnvironment());
    process_->setWorkingDirectory(workingDirectory_);

#ifdef Q_OS_WIN
    process_->setProgram(qEnvironmentVariable("COMSPEC", QStringLiteral("cmd.exe")));
    // QProcess would quote the line as one argument, which cmd.exe does not understand
    process_->setNativeArguments(QStringLiteral("/d /s /c \"%1\"").arg(line));
#else
    process_->setProgram(QStringLiteral("/bin/sh"));
    process_->setArguments({QStringLiteral("-c"), line});
    // its own process group: Ctrl+C / Stop reach the program and the programs started by it, not our editor
    process_->setChildProcessModifier([] {
        ::setsid();
        ::signal(SIGINT, SIG_DFL); // an editor started from a script may have SIGINT ignored, and children inherit that
    });
#endif

    connect(process_, &QProcess::readyReadStandardOutput, this, &ConsoleWidget::readAvailable);
    connect(process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        onProcessFinished(exitCode, static_cast<int>(status));
    });
    connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && process_)
            onProcessFailedToStart(process_->errorString());
    });

    updateRunningUi();
    emit runningChanged(true);
    process_->start();
}

void ConsoleWidget::readAvailable()
{
    if (!process_)
        return;
    const QByteArray bytes = process_->readAllStandardOutput();
    if (!bytes.isEmpty())
        appendOutput(decoder_(bytes));
}

void ConsoleWidget::onProcessFinished(int exitCode, int exitStatus)
{
    readAvailable(); // what the program wrote last

    finishCommandInfo(); // before the status line, which does not belong to the output of the command

    QProcess* finished = process_;
    process_ = nullptr;
    finished->deleteLater();

    const bool crashed = exitStatus == static_cast<int>(QProcess::CrashExit);
    if (crashed && interruptCount_ > 0)
        appendStatus(tr("[interrupted]"), /*error=*/false);
    else if (crashed)
        appendStatus(tr("[terminated abnormally]"), /*error=*/true);
    else if (exitCode != 0)
        appendStatus(tr("[exit code %1]").arg(exitCode), /*error=*/true);

    interruptCount_ = 0;
    updateRunningUi();
    emit runningChanged(false);
    emit commandFinished(crashed ? -1 : exitCode);
}

void ConsoleWidget::onProcessFailedToStart(const QString& reason)
{
    finishCommandInfo();

    QProcess* failed = process_;
    process_ = nullptr;
    failed->deleteLater();

    appendStatus(tr("[cannot start the shell: %1]").arg(reason), /*error=*/true);
    updateRunningUi();
    emit runningChanged(false);
    emit commandFinished(-1);
}

void ConsoleWidget::finishCommandInfo()
{
    commandTick_->stop();
    if (currentCommand_)
    {
        if (output_->document()->lastBlock().length() > 1) // the last line has no line break
            ++currentCommand_->lineCount;
        colorizeListing(/*includeLastLine=*/true);
        currentCommand_->durationMs = commandTimer_.elapsed();
        currentCommand_->running = false;
        currentCommand_.reset();
    }
    colorListing_ = false;
    output_->refreshMargin();
}

// ------------------------------------------------------------------ output

bool ConsoleWidget::darkBackground() const
{
    return output_->palette().base().color().lightness() < 128;
}

void ConsoleWidget::insertText(const QString& text, const QTextCharFormat& format)
{
    QScrollBar* bar = output_->verticalScrollBar();
    const bool followOutput = bar->value() >= bar->maximum() - 2; // the user scrolled up: do not pull the view down

    QTextCursor cursor(output_->document()); // our own cursor: the selection of the user stays
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);

    if (followOutput)
        bar->setValue(bar->maximum());
}

void ConsoleWidget::ensureAtLineStart()
{
    QTextCursor cursor(output_->document());
    cursor.movePosition(QTextCursor::End);
    if (cursor.positionInBlock() != 0)
        insertText(QStringLiteral("\n"), {});
}

void ConsoleWidget::eraseLastLine()
{
    QTextCursor cursor(output_->document());
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
}

void ConsoleWidget::appendOutput(QString text)
{
    // Progress bars (git, pip, cmake) rewrite their line with a lone '\r'; "\r\n" is an ordinary line break.
    // A chunk can end between the two characters of "\r\n", so the last '\r' waits for the next chunk.
    if (pendingCarriageReturn_)
    {
        text.prepend(QLatin1Char('\r'));
        pendingCarriageReturn_ = false;
    }
    if (text.endsWith(QLatin1Char('\r')))
    {
        text.chop(1);
        pendingCarriageReturn_ = true;
    }
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (currentCommand_)
        currentCommand_->lineCount += static_cast<int>(text.count(QLatin1Char('\n')));

    QScrollBar* bar = output_->verticalScrollBar();
    const bool followOutput = bar->value() >= bar->maximum() - 2;

    const QStringList parts = text.split(QLatin1Char('\r'));
    for (qsizetype i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
            eraseLastLine();
        if (!parts[i].isEmpty())
            writer_.append(output_, parts[i]);
    }

    colorizeListing(/*includeLastLine=*/false);
    output_->refreshMargin();

    if (followOutput)
        bar->setValue(bar->maximum());
}

void ConsoleWidget::colorizeListing(bool includeLastLine)
{
    if (!colorListing_)
        return;

    // the last line of the document is not complete yet (more of it can come), the others are
    QTextBlock block = listingCursor_.block();
    while (block.isValid() && (includeLastLine || block.next().isValid()))
    {
        if (classifiedLines_ < kMaxClassifiedLines)
        {
            colorizeListingLine(block);
            ++classifiedLines_;
        }
        block = block.next();
    }
    if (block.isValid())
        listingCursor_.setPosition(block.position());
}

void ConsoleWidget::colorizeListingLine(const QTextBlock& block)
{
    const QString text = block.text();
    if (text.isEmpty())
        return;

    // `ls --color=always` has its own colors: they stay
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
    {
        if (it.fragment().charFormat().hasProperty(QTextFormat::ForegroundBrush))
            return;
    }

    const auto range = listing_.directoryRange(text);
    if (!range)
        return;

    QTextCursor cursor(output_->document());
    cursor.setPosition(block.position() + static_cast<int>(range->start));
    cursor.setPosition(block.position() + static_cast<int>(range->start + range->length), QTextCursor::KeepAnchor);
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);
    format.setForeground(QColor(darkBackground() ? 0x6cb6ff : 0x1565c0));
    cursor.mergeCharFormat(format);
}

void ConsoleWidget::appendStatus(const QString& text, bool error)
{
    ensureAtLineStart();
    QTextCharFormat format;
    format.setForeground(error ? QColor(darkBackground() ? 0xff6b68 : 0xc62828) : QColor(darkBackground() ? 0x8a8a8a : 0x757575));
    insertText(text + QLatin1Char('\n'), format);
}

std::shared_ptr<ConsoleCommandInfo> ConsoleWidget::echoCommand(const QString& line, bool showStats)
{
    ensureAtLineStart();

    QTextCharFormat promptFormat;
    promptFormat.setFontWeight(QFont::Bold);
    promptFormat.setForeground(QColor(darkBackground() ? 0x7fd962 : 0x2e7d32));
    insertText(shortWorkingDirectory() + QStringLiteral(" $ "), promptFormat);

    QTextCharFormat commandFormat;
    commandFormat.setFontWeight(QFont::Bold);
    insertText(line + QLatin1Char('\n'), commandFormat);

    auto info = std::make_shared<ConsoleCommandInfo>();
    info->workingDirectory = workingDirectory_;
    info->showStats = showStats;
    output_->markCommandLine(info);
    return info;
}

void ConsoleWidget::echoStandardInput(const QString& text)
{
    ensureAtLineStart();
    QTextCharFormat format;
    format.setFontItalic(true);
    format.setForeground(QColor(darkBackground() ? 0x8a8a8a : 0x757575));
    insertText(text + QLatin1Char('\n'), format);
}

// ------------------------------------------------------------------ the prompt

QString ConsoleWidget::shortWorkingDirectory() const
{
    QString directory = QDir::cleanPath(workingDirectory_);
    const QString home = QDir::cleanPath(QDir::homePath());
    if (directory == home)
        return QStringLiteral("~");
    if (directory.startsWith(home + QLatin1Char('/')))
        directory = QStringLiteral("~") + directory.mid(home.size());

    constexpr qsizetype kMaxComponents = 3;
    const QStringList parts = directory.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() > kMaxComponents)
        directory = QStringLiteral("…/") + parts.mid(parts.size() - kMaxComponents).join(QLatin1Char('/'));
    return QDir::toNativeSeparators(directory);
}

void ConsoleWidget::updatePrompt()
{
    prompt_->setText(process_ ? QStringLiteral("▶") : shortWorkingDirectory() + QStringLiteral(" $"));
    prompt_->setToolTip(QDir::toNativeSeparators(workingDirectory_));
}

void ConsoleWidget::updateRunningUi()
{
    const bool running = process_ != nullptr;
    stopButton_->setVisible(running);
    input_->setPlaceholderText(running ? tr("Typed text goes to the program (Ctrl+D ends the input, Ctrl+C stops the program)")
                                       : tr("Type a command (Enter runs it, Ctrl+C stops it, Ctrl+L clears, Esc goes back to the editor)"));
    updatePrompt();
}

void ConsoleWidget::retranslate()
{
    stopButton_->setToolTip(tr("Stop the command (Ctrl+C); the second click kills it"));
    clearButton_->setToolTip(tr("Clear the console (Ctrl+L, or the command clear)"));
    updateRunningUi();
}

void ConsoleWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslate();
}
