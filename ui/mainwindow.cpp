#include <QKeyEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDesktopServices>
#include <QDockWidget>
#include <QClipboard>
#include <QStack>
#include <QScrollBar>
#include <QTimer>
#include <QTranslator>
#include <QPrinter>
#include <QPrintDialog>
#include <QAction>
#include <QActionGroup>
#include <QFontDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QTabBar>
#include <algorithm>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "ui/shortcutsdialog.h"
#include "ui/stctagsbuttons.h"
#include "ui/WorkAwareStopwatch.h"
#include "checkers/PairedTagsChecker.h"
#include "errorlist.h"
#include "types/documentstatistics.h"
#include "widgets/LoginDialog.h"
#include "widgets/ConsoleWidget.h"
#include "widgets/CppNewsWidget.h"
#include "widgets/DiffReviewDialog.h"
#include "widgets/PastedTextDiffDialog.h"
#include "widgets/FileDiffDialog.h"
#include "widgets/RenameFileDialog.h"
using namespace std;

namespace
{
constexpr int maxElementsInListOfLastElements = 10;

namespace GeometryNames
{
    constexpr const char GEOMETRY[] = "geometry";
    constexpr const char SETTINGS_WINDOW_STATE[] = "windowState";
    constexpr const char LAST_DIRECTORY[] = "lastDirectory";
    constexpr const char RECENT_FILES_LIST[] = "recentFiles";
    constexpr const char RESUME_SESSION[] = "startup/resumeSession";
    constexpr const char SESSION_OPEN_FILES[] = "session/openFiles";
    constexpr const char SESSION_ACTIVE_FILE[] = "session/activeFile";
    constexpr const char LOGIN_USERNAME[] = "login/username";
    constexpr const char LOGIN_PASSWORD[] = "login/password";
    constexpr const char LOGIN_REMEMBER[] = "login/remember";
    constexpr const char LOGIN_AUTO_LOGIN[] = "login/autoLogin";
    constexpr const char PREVIEW_SCROLL_SYNC[] = "preview/scrollSync";
    constexpr const char PREVIEW_CLICK_SYNC[] = "preview/clickSync";
    constexpr const char CONSOLE_HISTORY[] = "console/history";
};

std::pair<QString, QString> extractLink(const QString& text)
{
    static QRegularExpression regex(R"(https?://\S+)");
    QRegularExpressionMatch match = regex.match(text);

    if (match.hasMatch())
    {
        QString link = match.captured(0);
        int start = match.capturedStart();
        int end = match.capturedEnd();

        QString left = text.left(start).trimmed();
        QString right = text.mid(end).trimmed();

        QString description;

        if (!left.isEmpty() && !right.isEmpty())
        {
            description = left + " " + right;
        }
        else if (!left.isEmpty())
        {
            description = left;
        }
        else if (!right.isEmpty())
        {
            description = right;
        }

        return {link, description};
    }

    // No link found — return empty link and original text as description
    return {"", text};
}

QList<QPair<QString, MainWindow::RecentFileInfo>> getSortedExistingRecentFiles(const QMap<QString, MainWindow::RecentFileInfo>& recentFilesWithPositions)
{
    QList<QPair<QString, MainWindow::RecentFileInfo>> files;
    for (auto it = recentFilesWithPositions.begin(); it != recentFilesWithPositions.end(); ++it)
    {
        if (QFile::exists(it.key()))
            files.append({it.key(), it.value()});
    }

    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        return a.second.lastOpened > b.second.lastOpened;
    });

    return files;
}
} // namespace


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), m_translator(nullptr)
{    
    ui->setupUi(this);

    // The console is made before loadSettings(): restoreState() has to find its dock to restore its place and size
    consoleWidget = new ConsoleWidget(this);
    consoleWidget->setCurrentFileProvider([this] { return currentEditor()->getFileName(); });
    consoleDockWidget = new QDockWidget(tr("Console"), this);
    consoleDockWidget->setObjectName(QStringLiteral("consoleDockWidget"));
    consoleDockWidget->setWidget(consoleWidget);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDockWidget);

    // document tabs: closable, the current one is shown in the window title and drives every file action;
    // the tab bar is hidden while only one document is open
    ui->editorTabs->setTabsClosable(true);
    ui->editorTabs->setTabBarAutoHide(true);
    connect(ui->editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onEditorTabsCurrentChanged);
    connect(ui->editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onEditorTabCloseRequested);

    // the tabs can be reordered by dragging; a right click on the tab bar offers closing,
    // a middle click on a tab closes it (see eventFilter)
    ui->editorTabs->setMovable(true);
    QTabBar* tabBar = ui->editorTabs->tabBar();
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar, &QTabBar::customContextMenuRequested, this, &MainWindow::onEditorTabsTabBarContextMenu);
    tabBar->installEventFilter(this);

    // the first document tab; further ones are created by File > New / Open
    editorFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    editorFont.setPointSize(14);
    createEditorTab();

    loadSettings();

    updateStatusBarCounts(currentEditor());
    updateStatusBarPosition(currentEditor());

    setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/true);

    ui->stcPreviewDockWidget->hide();
    ui->stopwatchGroupBox->hide();
    consoleDockWidget->hide();

    ui->findWidget->hide();
    ui->findDockWidget->hide();
    ui->findWidget->setCodeEditor(currentEditor());
    currentEditor()->setFocus();

    ui->contextTableWidget->setTextEditor(currentEditor());
    ui->codesListTableWidget->setTextEditor(currentEditor());
    ui->todosTableWidget->setTextEditor(currentEditor());

    // the news of cpp0x.pl: a tab of the right panel, shown from the View menu
    cppNewsWidget = new CppNewsWidget(this);
    cppNewsTabIndex = ui->contextsTabWidget->addTab(cppNewsWidget, tr("cpp0x.pl news"));
    ui->contextsTabWidget->setTabVisible(cppNewsTabIndex, false);
    connect(ui->actionCpp0x_news, &QAction::toggled, this, &MainWindow::onCppNewsShowChanged);

    connect(ui->actionPrint, &QAction::triggered, this, &MainWindow::onPrintPressed);
    connect(ui->actionExport_pdf, &QAction::triggered, this, &MainWindow::onExportPdfPressed);
    connect(ui->actionWord_wrap, &QAction::toggled, this, &MainWindow::onWordWrapChanged);
    connect(ui->actionAuto_indent, &QAction::toggled, this, &MainWindow::onAutoIndentChanged);
    connect(ui->actionEditor_font, &QAction::triggered, this, &MainWindow::onEditorFontClicked);
    connect(ui->actionInsert_datetime, &QAction::triggered, this, &MainWindow::onInsertDatetimePressed);
    connect(ui->actionReopen_closed_tab, &QAction::triggered, this, &MainWindow::onReopenClosedTabPressed);
    connect(ui->actionNavigate_back, &QAction::triggered, this, &MainWindow::onNavigateBackPressed);
    connect(ui->actionNavigate_forward, &QAction::triggered, this, &MainWindow::onNavigateForwardPressed);
    connect(ui->actionCompare_with_file, &QAction::triggered, this, &MainWindow::onCompareWithFileRequested);

    // the counts of the document and the position of the cursor, on the right of the status bar
    statusCountsLabel = new QLabel(this);
    statusPositionLabel = new QLabel(this);
    ui->statusbar->addPermanentWidget(statusCountsLabel);
    ui->statusbar->addPermanentWidget(statusPositionLabel);

    // the console: View menu entry (the dock keeps its state), Ctrl+` shows it, Esc in its line goes back to the editor
    QAction* consoleAction = consoleDockWidget->toggleViewAction();
    consoleAction->setText(tr("Console"));
    consoleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft));
    ui->menuView->addSeparator();
    ui->menuView->addAction(consoleAction);
    connect(consoleDockWidget, &QDockWidget::visibilityChanged, this, &MainWindow::onConsoleVisibilityChanged);
    connect(consoleWidget, &ConsoleWidget::leaveRequested, this, [this] {
        if (currentEditor())
            currentEditor()->setFocus();
    });
    connect(consoleWidget, &ConsoleWidget::openDirectoryRequested, this, [](const QString& directory) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    connect(consoleWidget, &ConsoleWidget::historyChanged, this, [](const QStringList& history) {
        QSettings().setValue(GeometryNames::CONSOLE_HISTORY, history);
    });

    connectSignals2Slots();
    connectShortcuts();

    // Restore last session if the setting is enabled
    restoreLastSession();
}

void MainWindow::connectSignals2Slots()
{
    connect(ui->buttonsEmittingStc, &StcTagsButtons::buttonPressed, this, &MainWindow::onStcTagsButtonPressed);
    connect(ui->contextTableWidget, &FilteredTagTableWidget::goToLineClicked, this, [this](int line) {
        if (currentEditor()) currentEditor()->go2LineRequested(line);
    });
    connect(ui->goToLineGroupBox, &GoToLineWidget::onGoToLineRequested, this, [this](int line) {
        if (currentEditor()) currentEditor()->go2LineRequested(line);
    });
    connect(ui->findWidget, &FindDialog::jumpToLocationRequested, this, [this](int line, int offset) {
        if (currentEditor()) currentEditor()->goToLineAndOffset(line, offset);
    });

    // The preview follows the editor (the user scrolls only the editor)
    connect(ui->stcPreviewWidget, &StcPreviewWidget::scrollSyncEnabledChanged, this, [](bool enabled) {
        QSettings().setValue(GeometryNames::PREVIEW_SCROLL_SYNC, enabled);
    });

    // ... and a click in the preview puts the cursor of the editor in the clicked place
    connect(ui->stcPreviewWidget, &StcPreviewWidget::sourcePositionClicked, this, &MainWindow::onPreviewClicked);
    connect(ui->stcPreviewWidget, &StcPreviewWidget::clickSyncEnabledChanged, this, [](bool enabled) {
        QSettings().setValue(GeometryNames::PREVIEW_CLICK_SYNC, enabled);
    });
    connect(ui->todosTableWidget, &TodoTrackerTableWidget::todosTotalCountChanged, [this](int todosTotal) {
        this->setTodosCounterValue(todosTotal);
    });
    connect(ui->menuOpen_recent, &QMenu::aboutToShow, this, &MainWindow::onRecentRecentFilesMenuOpened);
    
    // Language switching connections
    connect(ui->actionEnglish, &QAction::triggered, this, &MainWindow::onLanguageChanged);
    connect(ui->actionPolish, &QAction::triggered, this, &MainWindow::onLanguageChanged);

    // Syntax switching: exactly one mode is checked at a time
    auto* syntaxActions = new QActionGroup(this);
    syntaxActions->setExclusive(true);
    for (const auto& [action, mode] : syntaxMenuActions())
    {
        syntaxActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { currentEditor()->setSyntaxModeChosenByUser(mode); });
    }
    onSyntaxModeChanged(currentEditor()->syntaxMode());

    setUpEncodingMenu();

    ui->breadcrumbTextBrowser->setTextEditor(currentEditor());
    ui->breadcrumbTextBrowser->setHeaderTable(ui->contextTableWidget);
}

void MainWindow::onStcTagsButtonPressed(StcTags stcTag)
{
    switch (stcTag)
    {
    case StcTags::RUN:
        toggleTagOnSelectedText(tagsClasses[StcTags::RUN]);
        break;
    case StcTags::BOLD:
        toggleTagOnSelectedText(tagsClasses[StcTags::BOLD]);
        break;
    case StcTags::ITALIC:
        toggleTagOnSelectedText(tagsClasses[StcTags::ITALIC]);
        break;
    case StcTags::UNDERLINED:
        toggleTagOnSelectedText(tagsClasses[StcTags::UNDERLINED]);
        break;
    case StcTags::STRUCK_OUT:
        toggleTagOnSelectedText(tagsClasses[StcTags::STRUCK_OUT]);
        break;
    case StcTags::H1:
        toggleTagOnSelectedText(tagsClasses[StcTags::H1]);
        break;
    case StcTags::H2:
        toggleTagOnSelectedText(tagsClasses[StcTags::H2]);
        break;
    case StcTags::H3:
        toggleTagOnSelectedText(tagsClasses[StcTags::H3]);
        break;
    case StcTags::H4:
        toggleTagOnSelectedText(tagsClasses[StcTags::H4]);
        break;
    case StcTags::CPP:
        surroundSelectedTextWithTag(tagsClasses[StcTags::CPP], tagsClasses[StcTags::CPP]);
        break;
    case StcTags::PY:
        surroundSelectedTextWithTag(tagsClasses[StcTags::PY], tagsClasses[StcTags::PY]);
        break;
    case StcTags::CODE:
        surroundSelectedTextWithTag(tagsClasses[StcTags::CODE], tagsClasses[StcTags::CODE]);
        break;
    case StcTags::DIV:
        surroundSelectedTextWithTag(tagsClasses[StcTags::DIV], tagsClasses[StcTags::DIV]);
        break;
    case StcTags::DIV_TIP:
        surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_TIP], tagsClasses[StcTags::DIV], R"( class="tip")");
        break;
    case StcTags::DIV_WARNING:
        surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_WARNING], tagsClasses[StcTags::DIV], R"( class="uwaga")");
        break;
    case StcTags::QUOTE:
        surroundSelectedTextWithTag(tagsClasses[StcTags::QUOTE], tagsClasses[StcTags::QUOTE]);
        break;
    case StcTags::A_HREF:
        this->surroundSelectedTextWithAHrefTag();
        break;
    case StcTags::IMG:
        this->surroundSelectedTextWithImgTag();
        break;
    case StcTags::PKT:
        surroundSelectedTextWithTag(tagsClasses[StcTags::PKT], tagsClasses[StcTags::PKT], " ext");
        break;
    case StcTags::CSV:
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::CSV], tagsClasses[StcTags::CSV], " extended header");
        break;
    case StcTags::SUBSCRIPT:
        surroundSelectedTextWithTag(tagsClasses[StcTags::SUBSCRIPT], tagsClasses[StcTags::SUBSCRIPT]);
        break;
    case StcTags::SUPSCRIPT:
        surroundSelectedTextWithTag(tagsClasses[StcTags::SUPSCRIPT], tagsClasses[StcTags::SUPSCRIPT]);
        break;
    case StcTags::TELE_TYPE:
        surroundSelectedTextWithTag(tagsClasses[StcTags::TELE_TYPE], tagsClasses[StcTags::TELE_TYPE]);
        break;
    default:
        qDebug() << __FILE__ << ": " << __LINE__ << ": Unsupported option: " << std::to_underlying(stcTag);
        break;
    }
}
void MainWindow::toggleTagOnSelectedText(const QString& tag)
{
    auto cursor = currentEditor()->textCursor();
    QString selectedText = cursor.selectedText();

    // Handle newlines as QChar::ParagraphSeparator
    QString openTag = "[" + tag + "]";
    QString closeTag = "[/" + tag + "]";

    // Remove leading/trailing spaces from selection
    QString trimmedText = selectedText.trimmed();
    int leadingSpaces = selectedText.indexOf(trimmedText);
    int trailingSpaces = selectedText.length() - trimmedText.length() - leadingSpaces;

    // 1. Case: Selection already includes tags -> remove tags
    if (trimmedText.startsWith(openTag) && trimmedText.endsWith(closeTag))
    {
        // Remove tags from selection
        QString untagged = trimmedText.mid(openTag.length(), trimmedText.length() - openTag.length() - closeTag.length());
        QString result = QString(selectedText.left(leadingSpaces)) + untagged + QString(selectedText.right(trailingSpaces));
        cursor.insertText(result);
        return;
    }

    // 2. Case: Selection is inside tags (e.g. only 'bold' in '[b]bold[/b]')
    if (!selectedText.isEmpty())
    {
        int selStart = cursor.selectionStart();
        int selEnd = cursor.selectionEnd();
        QString fullText = currentEditor()->toPlainText(); // TODO: Can't we avoid merging entire text?
        // Check for openTag before selection and closeTag after selection
        int openTagStart = selStart - openTag.length();
        int closeTagEnd = selEnd + closeTag.length();
        if (openTagStart >= 0 && closeTagEnd <= fullText.length())
        {
            if (fullText.mid(openTagStart, openTag.length()) == openTag &&
                fullText.mid(selEnd, closeTag.length()) == closeTag)
            {
                // Remove closeTag first
                QTextCursor tmpCursor = currentEditor()->textCursor();
                tmpCursor.setPosition(selEnd);
                tmpCursor.setPosition(selEnd + closeTag.length(), QTextCursor::KeepAnchor);
                tmpCursor.removeSelectedText();
                // Remove openTag
                tmpCursor.setPosition(openTagStart);
                tmpCursor.setPosition(openTagStart + openTag.length(), QTextCursor::KeepAnchor);
                tmpCursor.removeSelectedText();
                // Restore original text (without tags)
                QTextCursor restoreCursor = currentEditor()->textCursor();
                restoreCursor.setPosition(openTagStart);
                restoreCursor.setPosition(openTagStart + selectedText.length(), QTextCursor::KeepAnchor);
                currentEditor()->setTextCursor(restoreCursor);
                return;
            }
        }
    }

    // 3. Default: Surround selection with tags
    QString result = openTag + trimmedText + closeTag;
    result = QString(selectedText.left(leadingSpaces)) + result + QString(selectedText.right(trailingSpaces));
    cursor.insertText(result);
    
    // If no text was selected, position cursor between tags
    if (selectedText.isEmpty()) 
    {
        int pos = cursor.position() - closeTag.length();
        cursor.setPosition(pos);
        currentEditor()->setTextCursor(cursor);
    }
}

void MainWindow::onRecentRecentFilesMenuOpened()
{
    ui->menuOpen_recent->clear();

    const auto sortedRecentFiles = getSortedExistingRecentFiles(recentFilesWithPositions);

    int filesAdded = 0;
    for (const auto& [filePath, info] : sortedRecentFiles)
    {
        if (++filesAdded > maxElementsInListOfLastElements)
            break;

        ui->menuOpen_recent->addAction(createRecentFileAction(filePath, info));
    }

    if (filesAdded == 0)
        addEmptyRecentFilesLabel();
    else
        addClearRecentFilesAction();
}

QAction* MainWindow::createRecentFileAction(const QString& filePath, const RecentFileInfo& fileInfo)
{
    QString fileName = QFileInfo(filePath).fileName();

    QString tooltip = QString("%1\n\nRecently opened on: %2")
                          .arg(filePath)
                          .arg(fileInfo.lastOpened.toString("dd.MM.yyyy hh:mm:ss"));

    QString label = QString("%1   [last opened: %2]")
                        .arg(fileName)
                        .arg(fileInfo.lastOpened.toString("dd.MM.yy hh:mm"));

    QAction* action = new QAction(label, this);
    action->setToolTip(tooltip);
    action->setData(filePath);

    connect(action, &QAction::triggered, this, [this, filePath]() {
        if (!QFile::exists(filePath))
        {
            QMessageBox::warning(this, tr("File not found"), tr("File does not exist:\n") + filePath);
            return;
        }

        openFileInTab(filePath);

        const int position = recentFilesWithPositions.value(filePath).cursorPosition;
        const int scrollPosition = recentFilesWithPositions.value(filePath).scrollPosition;
        if (currentEditor())
        {
            QTextCursor cursor = currentEditor()->textCursor();
            cursor.setPosition(position);
            currentEditor()->setTextCursor(cursor);
            currentEditor()->ensureCursorVisible();
            if (scrollPosition > 0)
                currentEditor()->verticalScrollBar()->setValue(scrollPosition);
            updateRecentFiles(filePath);
        }
        onRecentRecentFilesMenuOpened();
    });

    return action;
}

void MainWindow::addEmptyRecentFilesLabel()
{
    QAction* noFilesAction = new QAction(tr("No recent files"), this);
    noFilesAction->setEnabled(false);
    ui->menuOpen_recent->addAction(noFilesAction);
}

void MainWindow::addClearRecentFilesAction()
{
    ui->menuOpen_recent->addSeparator();

    QAction* clearAction = new QAction(tr("Clear Recent Files"), this);
    connect(clearAction, &QAction::triggered, this, [this]() {
        recentFilesWithPositions.clear();
        onRecentRecentFilesMenuOpened();
    });

    ui->menuOpen_recent->addAction(clearAction);
}


void MainWindow::onCopyFileAbsoluteNamePressed()
{
    const auto fileName = currentEditor()->getFileName();
    if (! fileName.isEmpty())
    {
        const auto fileNameAbsolutePath = QFileInfo(fileName).absoluteFilePath();
        QApplication::clipboard()->setText(fileNameAbsolutePath);
    }
}

void MainWindow::onCopyFileBaseNamePressed()
{
    const auto fileName = currentEditor()->getFileName();
    if (! fileName.isEmpty())
    {
        QString baseName = QFileInfo(fileName).fileName();
        QApplication::clipboard()->setText(baseName);
    }
}

void MainWindow::onOpenParentDirectoryPressed()
{
    QString fileName = currentEditor()->getFileName();
    QString pathToOpen;

    if (fileName.isEmpty())
    {
        pathToOpen = QDir::currentPath();
    }
    else
    {
        pathToOpen = QFileInfo(fileName).absolutePath();
    }

    // Open in system file explorer
    QDesktopServices::openUrl(QUrl::fromLocalFile(pathToOpen));
}

void MainWindow::onShowAvailableShortcutsPressed()
{
    auto *dialog = new ShortcutsDialog(this);

    dialog->addShortcuts(ui->buttonsEmittingStc->listOfShortcuts(), "Buttons");

    dialog->addShortcuts(currentEditor()->listOfShortcuts(), "Editor");

    QList<QAction*> menuActions = findChildren<QAction*>();
    dialog->addQActions(menuActions, "Application Menu");

    dialog->adjustSizeToContents();

    dialog->exec();
}

void MainWindow::onFileStatsRequested()
{
    auto result = DocumentStatistics::analyze(currentEditor());
    QMessageBox::information(this, tr("File statistics"), result.toQString());
}

void MainWindow::onCompareWithPastedTextRequested()
{
    PastedTextDiffDialog dialog(currentEditor(), this);
    dialog.exec();
}

void MainWindow::onFindTriggered(bool checked)
{
    if (checked)
    {
        ui->findWidget->setReplaceMode(false);
        ui->findDockWidget->setVisible(true);
        ui->findWidget->setVisible(true);
        ui->findWidget->focusInput();
    }
    else
    {
        ui->findDockWidget->setVisible(false);
        currentEditor()->setFocus();
    }

    QSignalBlocker blocker(ui->actionFind_replace);
    ui->actionFind_replace->setChecked(false);
}

void MainWindow::onFindReplaceTriggered(bool checked)
{
    if (checked)
    {
        ui->findWidget->setReplaceMode(true);
        ui->findDockWidget->setVisible(true);
        ui->findWidget->setVisible(true);
        ui->findWidget->focusInput(true);
    }
    else
    {
        ui->findDockWidget->setVisible(false);
        currentEditor()->setFocus();
    }

    QSignalBlocker blocker(ui->actionFind);
    ui->actionFind->setChecked(false);
}

CodeEditor* MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor*>(ui->editorTabs->currentWidget());
}

CodeEditor* MainWindow::createEditorTab()
{
    auto* editor = new CodeEditor(ui->editorTabs);

    // the same look the editor had as a single widget in mainwindow.ui
    editor->setFont(editorFont);
    editor->setStyleSheet(QStringLiteral("QPlainText {\n"
                                         "\tbackground-color: rgb(255, 255, 255);\n"
                                         "\tcolor: rgb(0, 0, 0);\n"
                                         "}\n"));
    editor->setLineWrapMode(wordWrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    editor->setAutoIndentEnabled(autoIndentEnabled);

    const int index = ui->editorTabs->addTab(editor, tr("Untitled"));
    connectEditorSignals(editor);
    updateTabTitle(editor);
    Q_UNUSED(index);
    return editor;
}

void MainWindow::connectEditorSignals(CodeEditor* editor)
{
    // the widgets below show the document of the current tab: signals of the other tabs are ignored
    connect(editor, &CodeEditor::totalLinesCountChanged, this, [this, editor](int lines) {
        if (editor == currentEditor())
            ui->goToLineGroupBox->setMaxLine(lines);
    });
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateBreadcrumb);

    // the counts and the cursor position of the status bar follow the document of the current tab
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, editor]() {
        if (editor == currentEditor())
            updateStatusBarPosition(editor);
    });
    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
        if (editor == currentEditor())
        {
            scheduleStatusBarCountsUpdate();
            updateStatusBarPosition(editor);
        }
    });

    // The preview follows the editor (the user scrolls only the editor)
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::onEditorViewportMoved);
    connect(editor, &QPlainTextEdit::textChanged, this, &MainWindow::onEditorViewportMoved);

    connect(editor, &CodeEditor::numberOfModifiedLinesChanged, [this, editor](int linesNumber) {
        if (editor == currentEditor())
            this->onFileContentChanged(editor->getFileName(), linesNumber);
    });
    connect(editor, &QPlainTextEdit::textChanged, ui->stopwatchGroupBox, &WorkAwareStopwatch::notifyWorkActivity);
    connect(editor, &CodeEditor::contentReloaded, ui->todosTableWidget, &TodoTrackerTableWidget::scanEntireDocumentDetectingAllTodos);
    connect(editor, &CodeEditor::syntaxModeChanged, this, [this, editor](SyntaxMode mode) {
        if (editor == currentEditor())
            onSyntaxModeChanged(mode);
    });

    // the rendered preview of the STC markup follows the text of the current document
    connect(editor, &CodeEditor::textChanged, [this, editor]() {
        if (editor == currentEditor() && ui->stcPreviewWidget->isVisible())
            ui->stcPreviewWidget->updateText(editor->toPlainText());
    });

    // the "*" of the tab title and the modification info of the window title
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor](bool) {
        updateTabTitle(editor);
        if (editor == currentEditor())
        {
            const QString fileName = editor->getFileName();
            if (editor->document()->isModified())
                updateWindowTitle(fileName, editor->getFileModificationInfoText());
            else
                updateWindowTitle(fileName);
        }
    });

    connect(editor, &CodeEditor::shortcutPressed_bold, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::BOLD], tagsClasses[StcTags::BOLD]);
    });
    connect(editor, &CodeEditor::shortcutPressed_run, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::RUN], tagsClasses[StcTags::RUN]);
    });
    connect(editor, &CodeEditor::shortcutPressed_warning, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_WARNING], tagsClasses[StcTags::DIV], R"( class="uwaga")");
    });
    connect(editor, &CodeEditor::shortcutPressed_tip, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_TIP], tagsClasses[StcTags::DIV], R"( class="tip")");
    });
    connect(editor, &CodeEditor::shortcutPressed_href, [this]() {
        this->surroundSelectedTextWithAHrefTag();
    });

    connect(editor, &CodeEditor::shortcutPressed_h1, [this] {
        this->surroundSelectedTextWithTag("h1", "h1");
    });
    connect(editor, &CodeEditor::shortcutPressed_h2, [this] {
        this->surroundSelectedTextWithTag("h2", "h2");
    });
    connect(editor, &CodeEditor::shortcutPressed_h3, [this] {
        this->surroundSelectedTextWithTag("h3", "h3");
    });
    connect(editor, &CodeEditor::shortcutPressed_h4, [this] {
        this->surroundSelectedTextWithTag("h4", "h4");
    });
}

void MainWindow::updateTabTitle(CodeEditor* editor)
{
    const int index = ui->editorTabs->indexOf(editor);
    if (index < 0)
        return;

    const QString fileName = editor->getFileName();
    QString title = fileName.isEmpty() ? tr("Untitled") : QFileInfo(fileName).fileName();
    if (editor->document()->isModified())
        title = QLatin1Char('*') + title;

    ui->editorTabs->setTabText(index, title);
    ui->editorTabs->setTabToolTip(index, fileName);
}

void MainWindow::onEditorTabsCurrentChanged(int index)
{
    auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(index));
    if (!editor)
        return;

    refreshEditorDependentWidgets(editor);
    setDisabledMenuActionsDependingOnOpenedFile(editor->getFileName().isEmpty());
    onSyntaxModeChanged(editor->syntaxMode());
    ui->goToLineGroupBox->setMaxLine(editor->linesCount());
    updateEncodingMenuHeader();

    if (editor->document()->isModified())
        updateWindowTitle(editor->getFileName(), editor->getFileModificationInfoText());
    else
        updateWindowTitle(editor->getFileName());

    if (ui->stcPreviewWidget->isVisible())
        ui->stcPreviewWidget->updateText(editor->toPlainText());

    updateStatusBarCounts(editor);
    updateStatusBarPosition(editor);
}

void MainWindow::updateStatusBarCounts(CodeEditor* editor)
{
    if (!statusCountsLabel)
        return;
    if (!editor)
    {
        statusCountsLabel->clear();
        return;
    }

    // count the words without splitting the text into a list of them: the documents can be long
    int words = 0;
    bool insideWord = false;
    const QString text = editor->toPlainText();
    for (const QChar ch : text)
    {
        if (ch.isSpace())
            insideWord = false;
        else if (!insideWord)
        {
            insideWord = true;
            ++words;
        }
    }

    statusCountsLabel->setText(tr("%1 words · %2 chars").arg(words).arg(editor->document()->characterCount()));
}

void MainWindow::scheduleStatusBarCountsUpdate()
{
    if (!statusCountsTimer)
    {
        statusCountsTimer = new QTimer(this);
        statusCountsTimer->setSingleShot(true);
        connect(statusCountsTimer, &QTimer::timeout, this, [this]() {
            updateStatusBarCounts(currentEditor());
        });
    }
    statusCountsTimer->start(250);
}

void MainWindow::updateStatusBarPosition(CodeEditor* editor)
{
    if (!statusPositionLabel)
        return;
    if (!editor)
    {
        statusPositionLabel->clear();
        return;
    }

    const QTextCursor cursor = editor->textCursor();
    statusPositionLabel->setText(tr("Ln %1, Col %2").arg(cursor.blockNumber() + 1).arg(cursor.columnNumber() + 1));
}

void MainWindow::refreshEditorDependentWidgets(CodeEditor* editor)
{
    ui->contextTableWidget->setTextEditor(editor);
    ui->codesListTableWidget->setTextEditor(editor);
    ui->todosTableWidget->setTextEditor(editor);
    ui->breadcrumbTextBrowser->setTextEditor(editor);
    ui->findWidget->setCodeEditor(editor);
}

void MainWindow::onEditorTabCloseRequested(int index)
{
    ui->editorTabs->setCurrentIndex(index);
    confirmAndCloseCurrentTab();
}

bool MainWindow::confirmAndCloseCurrentTab(bool ensureAtLeastOneTab)
{
    auto* editor = currentEditor();
    if (!editor)
        return false;

    if (!operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Close tab without saving?"), tr("Do You really want to close this tab without saving unsaved changes?"),
            /*restoreEditorAfterwards=*/false))
    {
        return false;
    }

    rememberClosedTab(editor);

    const int index = ui->editorTabs->indexOf(editor);
    ui->editorTabs->removeTab(index);
    editor->deleteLater();

    if (ensureAtLeastOneTab && ui->editorTabs->count() == 0)
        createEditorTab();

    // removing the current tab fires currentChanged(), which refreshes the title for the tab shown next
    return true;
}

void MainWindow::rememberClosedTab(CodeEditor* editor)
{
    const QString fileName = editor->getFileName();
    // the content is kept only for a document without a name: a named one is read from the disk again
    const QString untitledText = fileName.isEmpty() ? editor->exactText() : QString();
    if (fileName.isEmpty() && untitledText.isEmpty())
        return; // an untouched "Untitled" tab is not worth reopening

    ClosedTabInfo info;
    info.fileName = fileName;
    info.untitledText = untitledText;
    info.cursorPosition = editor->textCursor().position();
    info.scrollValue = editor->verticalScrollBar()->value();

    closedTabs.append(info);
    while (closedTabs.size() > 10)
        closedTabs.removeFirst();
}

void MainWindow::restoreClosedTabPosition(CodeEditor* editor, int cursorPosition, int scrollValue)
{
    // after the event loop, so the tab is shown and the scrollbar has its range
    QTimer::singleShot(0, editor, [editor, cursorPosition, scrollValue]() {
        QTextCursor cursor = editor->textCursor();
        cursor.setPosition(qBound(0, cursorPosition, editor->document()->characterCount() - 1));
        editor->setTextCursor(cursor);
        editor->ensureCursorVisible();
        editor->verticalScrollBar()->setValue(qMax(0, scrollValue));
    });
}

void MainWindow::onReopenClosedTabPressed()
{
    if (closedTabs.isEmpty())
        return;

    const ClosedTabInfo info = closedTabs.takeLast();
    CodeEditor* editor = nullptr;

    if (!info.fileName.isEmpty())
    {
        openFileInTab(info.fileName);
        editor = currentEditor();
    }
    else
    {
        editor = createEditorTab();
        if (!info.untitledText.isEmpty())
            editor->setPlainText(info.untitledText);
    }

    if (editor)
        restoreClosedTabPosition(editor, info.cursorPosition, info.scrollValue);
}

void MainWindow::onNavigateBackPressed()
{
    if (auto* editor = currentEditor())
        editor->navigateBack();
}

void MainWindow::onNavigateForwardPressed()
{
    if (auto* editor = currentEditor())
        editor->navigateForward();
}

void MainWindow::onCompareWithFileRequested()
{
    const QString filePath = chooseFileWithDialog(QFileDialog::AcceptOpen);
    if (filePath.isEmpty())
        return;

    FileDiffDialog dialog(currentEditor(), filePath, this);
    dialog.exec();
}

void MainWindow::onEditorTabsTabBarContextMenu(const QPoint& pos)
{
    QTabBar* tabBar = ui->editorTabs->tabBar();
    const int index = tabBar->tabAt(pos);
    if (index < 0)
        return;

    QMenu menu(this);
    QAction* closeAction = menu.addAction(tr("Close tab"));
    QAction* closeOthersAction = menu.addAction(tr("Close other tabs"));
    QAction* closeAllAction = menu.addAction(tr("Close all tabs"));

    const QAction* chosen = menu.exec(tabBar->mapToGlobal(pos));
    if (chosen == closeAction)
    {
        ui->editorTabs->setCurrentIndex(index);
        confirmAndCloseCurrentTab();
    }
    else if (chosen == closeOthersAction)
    {
        closeOtherTabsThan(index);
    }
    else if (chosen == closeAllAction)
    {
        closeAllTabs();
    }
}

void MainWindow::closeOtherTabsThan(int keepIndex)
{
    CodeEditor* keepEditor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(keepIndex));

    // from the last tab down, so the removal does not shift the indexes still to be visited
    for (int i = ui->editorTabs->count() - 1; i >= 0; --i)
    {
        if (ui->editorTabs->widget(i) == keepEditor)
            continue;
        ui->editorTabs->setCurrentIndex(i);
        if (!confirmAndCloseCurrentTab(/*ensureAtLeastOneTab=*/false))
            break; // the user cancelled: the remaining tabs stay open
    }

    const int restoredIndex = keepEditor ? ui->editorTabs->indexOf(keepEditor) : -1;
    if (restoredIndex >= 0)
        ui->editorTabs->setCurrentIndex(restoredIndex);
}

void MainWindow::closeAllTabs()
{
    while (ui->editorTabs->count() > 0)
    {
        ui->editorTabs->setCurrentIndex(0);
        if (!confirmAndCloseCurrentTab(/*ensureAtLeastOneTab=*/false))
            break;
    }
    if (ui->editorTabs->count() == 0)
        createEditorTab();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->editorTabs->tabBar() && event->type() == QEvent::MouseButtonRelease)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::MiddleButton)
        {
            const int index = ui->editorTabs->tabBar()->tabAt(mouseEvent->position().toPoint());
            if (index >= 0)
            {
                ui->editorTabs->setCurrentIndex(index);
                confirmAndCloseCurrentTab();
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

int MainWindow::findTabIndexForFile(const QString& fileName) const
{
    const auto targetInfo = QFileInfo(fileName);
    const auto targetCanonical = targetInfo.canonicalFilePath();
    const auto targetAbsolute = targetInfo.absoluteFilePath();

    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        const auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i));
        if (!editor)
            continue;

        const auto openInfo = QFileInfo(editor->getFileName());
        if (openInfo.fileName().isEmpty())
            continue;

        if ((!targetCanonical.isEmpty() && openInfo.canonicalFilePath() == targetCanonical) ||
            openInfo.absoluteFilePath() == targetAbsolute)
        {
            return i;
        }
    }
    return -1;
}

void MainWindow::openFileInTab(const QString& fileName)
{
    const int existingIndex = findTabIndexForFile(fileName);
    if (existingIndex >= 0)
    {
        ui->editorTabs->setCurrentIndex(existingIndex);
        return;
    }

    // a pristine unnamed tab takes the file instead of adding one more tab
    CodeEditor* editor = nullptr;
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        auto* candidate = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i));
        if (candidate && candidate->getFileName().isEmpty() && candidate->document()->isEmpty())
        {
            editor = candidate;
            ui->editorTabs->setCurrentIndex(i);
            break;
        }
    }

    const bool addedNewTab = (editor == nullptr);
    if (addedNewTab)
    {
        editor = createEditorTab();
        ui->editorTabs->setCurrentIndex(ui->editorTabs->indexOf(editor));
    }

    if (!loadFileContentToEditorDistargingCurrentContent(fileName) && addedNewTab)
    {
        // nothing was loaded into the fresh tab: remove it again
        const int index = ui->editorTabs->indexOf(editor);
        ui->editorTabs->removeTab(index);
        editor->deleteLater();
        if (ui->editorTabs->count() == 0)
            createEditorTab();
    }
}

void MainWindow::connectShortcuts()
{
    QAction *focusGo2LineWidgetAction = new QAction("Focus go2line", this);
    focusGo2LineWidgetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    addAction(focusGo2LineWidgetAction);
    connect(focusGo2LineWidgetAction, &QAction::triggered, [this]() {
        ui->goToLineGroupBox->setFocus();
        ui->goToLineGroupBox->setVisible(true);
        ui->actionGo_to_line->setChecked(true);
    });

    QAction *closeTabAction = new QAction(tr("Close tab"), this);
    closeTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    addAction(closeTabAction);
    connect(closeTabAction, &QAction::triggered, this, [this]() {
        confirmAndCloseCurrentTab();
    });
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (Qt::Key_Escape == event->key())
    {
        if (bool isFindVisible = ui->findDockWidget->isVisible())
        {
            ui->findDockWidget->hide();
            QSignalBlocker findBlocker(ui->actionFind);
            QSignalBlocker replaceBlocker(ui->actionFind_replace);
            ui->actionFind->setChecked(false);
            ui->actionFind_replace->setChecked(false);
        }
        else
        {
            closeApplicationReturningIfClosed();
        }
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // every tab is closed; only the ones with unsaved changes are asked about
    while (ui->editorTabs->count() > 0)
    {
        if (!confirmAndCloseCurrentTab(/*ensureAtLeastOneTab=*/false))
        {
            event->ignore();
            return;
        }
    }

    QWidget::closeEvent(event);
}

bool MainWindow::operationWhichDiscardsChangesRequestedReturningIfDiscarded(const QString &dialogTitle, const QString &dialogMessage,
                                                                            bool restoreEditorAfterwards)
{
    if (!currentEditor()->noUnsavedChanges())
    {
        DiffReviewDialog dialog(currentEditor(), dialogTitle, dialogMessage, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            switch (dialog.userChoice())
            {
            case DiffReviewDialog::Save:
                if (onSavePressed())
                {
                    // The file is saved, so there is nothing to bring back; the editor content is replaced (or closed) next anyway
                    if (restoreEditorAfterwards)
                        currentEditor()->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
                    return true;
                }
                else // saving failed
                {
                    return false;
                }
                break;
            case DiffReviewDialog::Discard:
                if (restoreEditorAfterwards)
                    currentEditor()->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
                else
                    currentEditor()->discardUnsavedChanges(); // no reload from disk: the caller throws the content away anyway
                break;
            case DiffReviewDialog::Cancel:
            default:
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool MainWindow::closeApplicationReturningIfClosed()
{
    // closeEvent() asks about the unsaved changes
    return close();
}

void MainWindow::onNewFilePressed()
{
    // a pristine unnamed tab is reused; every document with content stays open in its own tab
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i));
        if (editor && editor->getFileName().isEmpty() && editor->document()->isEmpty())
        {
            editor->newEmptyFile();
            ui->editorTabs->setCurrentIndex(i);
            editor->setFocus();
            return;
        }
    }

    auto* editor = createEditorTab();
    ui->editorTabs->setCurrentIndex(ui->editorTabs->indexOf(editor));
    editor->setFocus();
}

bool MainWindow::onSaveAsPressed()
{
    const auto fileName = chooseFileWithDialog(QFileDialog::AcceptSave);
    return saveEntireContent2File(fileName);
}

void MainWindow::onPrintPressed()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print"));
    if (dialog.exec() == QDialog::Accepted)
    {
        currentEditor()->print(&printer);
    }
}

void MainWindow::onExportPdfPressed()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export to PDF"),
                                                    lastDirectory.isEmpty() ? QDir::homePath() : lastDirectory,
                                                    tr("PDF document (*.pdf)"));
    if (fileName.isEmpty())
    {
        return;
    }
    if (! fileName.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive))
    {
        fileName += QStringLiteral(".pdf");
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    currentEditor()->print(&printer);

    lastDirectory = QFileInfo(fileName).absolutePath();
}

void MainWindow::onInsertDatetimePressed()
{
    currentEditor()->textCursor().insertText(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")));
}

QString MainWindow::chooseFileWithDialog(QFileDialog::AcceptMode acceptMode)
{
    QFileDialog dialog(this);
    QStringList nameFilters;
    nameFilters << QWidget::tr("Text file (*.txt)")
                << tr("Source and data files (%1)").arg(syntaxmode::knownSourceFileWildcards().join(' '))
                << tr("All files (*)");
    dialog.setNameFilters(nameFilters);
    dialog.setAcceptMode(acceptMode);
    dialog.setDefaultSuffix(".txt");
    if (! lastDirectory.isEmpty())
    {
        dialog.setDirectory(lastDirectory);
    }

    if (dialog.exec())
    {
        const QString fileName = dialog.selectedFiles()[0].trimmed();
        if (! fileName.isEmpty())
        {
            lastDirectory = QFileInfo(fileName).absolutePath();
            updateRecentFiles(fileName);
        }
        return fileName;
    }
    return {};
}

bool MainWindow::saveEntireContent2File(QString fileName)
{
    if (!fileName.isEmpty())
    {
        updateWindowTitle(fileName);
        if (currentEditor()->saveEntireContent2File(fileName))
        {
            updateTabTitle(currentEditor());
            setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/false);
            return true;
        }
    }
    return false;
}

void MainWindow::updateWindowTitle(QString fileName, QString suffix)
{
    QString title = qApp->applicationName();
    if (!fileName.isEmpty())
    {
        title += ": " + fileName;
        if (! suffix.isEmpty())
        {
            title += " \t[" + suffix + ']';
        }
    }

    // the number of open documents is shown only when there is more than one
    const int documentsCount = ui->editorTabs->count();
    if (documentsCount > 1)
    {
        title += QStringLiteral(" (%1)").arg(documentsCount);
    }

    setWindowTitle(title);
}

bool MainWindow::onSavePressed()
{
    auto outputFileName = currentEditor()->getFileName();
    if (outputFileName.isEmpty())
    {
        return onSaveAsPressed();
    }
    else if (currentEditor()->noUnsavedChanges())
    {
        return true;
    }
    else
    {
        return saveEntireContent2File(outputFileName);
    }
}

void MainWindow::onOpenPressed()
{
    const auto fileName = chooseFileWithDialog(QFileDialog::AcceptOpen).trimmed();
    if (! fileName.isEmpty())
    {
        openFileInTab(fileName);
        if (currentEditor())
            currentEditor()->setFocus();
    }
}

void MainWindow::onReloadFilePressed()
{
    if (currentEditor()->getFileName().isEmpty())
    {
        return;
    }

    if (! operationWhichDiscardsChangesRequestedReturningIfDiscarded(
        tr("Confirm reloading and loosing unsaved content"), tr("Do You really want to reload the currently opened file?\n"
                                                                "Currently unsaved changes would be declined."),
        /*restoreEditorAfterwards=*/false))
    {
        return;
    }

    currentEditor()->reloadFromFile(/*discardChanges=*/true);
}

void MainWindow::onRenameFilePressed()
{
    const auto oldFilePath = currentEditor()->getFileName();

    RenameFileDialog dialog(currentEditor(), this);
    if (QDialog::Accepted != dialog.exec())
        return;

    const auto newFilePath = dialog.newAbsoluteFilePath();

    updateWindowTitle(newFilePath);
    updateTabTitle(currentEditor());
    updateRecentFiles(newFilePath);

    QMessageBox::information(this, tr("Success"), tr("Successfull rename of file from:\n")
                             + oldFilePath + "\nto\n" + newFilePath);
}

bool MainWindow::loadFileContentToEditorDistargingCurrentContent(const QString& fileName)
{
    ui->todosTableWidget->clearTodos();

    if (currentEditor()->loadFileContentDistargingCurrentContent(fileName))
    {
        setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/false);

        updateWindowTitle(fileName);
        updateTabTitle(currentEditor());

        ui->actionReload_file->setEnabled(true);
        updateRecentFiles(fileName);

        ui->contextTableWidget->rebuildAllHeaders();

        setTodosCounterValue(ui->todosTableWidget->getTodosTotalCount());

        return true;
    }

    QMessageBox::warning(this, tr("Error opening file"), tr("File '") + fileName + tr("' failed to be opened from commandline!"));

    return false;
}

void MainWindow::setTodosCounterValue(int todosTotal)
{
    ui->contextsTabWidget->setTabText(2, tr("TODOs (") + QString::number(todosTotal) + ")");
}

void MainWindow::onExitFromApplicationMenuPressed()
{
    close();
}

void MainWindow::onStcCoursePressed()
{
    const QUrl url("https://cpp0x.pl/kursy/Kurs-STC/169");
    if (url.isValid())
        QDesktopServices::openUrl(url);
}
void MainWindow::onCpp0xPl_pressed()
{
    const QUrl url("https://cpp0x.pl");
    if (url.isValid())
        QDesktopServices::openUrl(url);
}
void MainWindow::onRepository_pressed()
{
    const QUrl url("https://github.com/baziorek/STC_editor/");
    if (url.isValid())
        QDesktopServices::openUrl(url);
}

void MainWindow::onCheckTagsPressed()
{
    const auto text = currentEditor()->toPlainText().toStdString();
    const auto tagsErrors = PairedTagsChecker::checkTags(text);

    ui->errorsInText->clearErrors();
    for (const auto [lineNumber, positionInLine, errorText] : tagsErrors)
    {
        ui->errorsInText->addError(lineNumber, positionInLine, QString::fromStdString(errorText));
    }
}

void MainWindow::onContextShowChanged(bool visible)
{
    ui->contextsTabWidget->setVisible(visible);
    ui->contextTableWidget->setVisible(visible);
}

void MainWindow::onCppNewsShowChanged(bool visible)
{
    ui->contextsTabWidget->setTabVisible(cppNewsTabIndex, visible);
    if (visible)
        ui->contextsTabWidget->setCurrentIndex(cppNewsTabIndex);
}

void MainWindow::onGoToLineShowChanged(bool visible)
{
    ui->goToLineGroupBox->setVisible(visible);
}
void MainWindow::onBreadcrumbVisibilityChanged(bool visible)
{
    ui->breadcrumbTextBrowser->setVisible(visible);
    if (ui->breadcrumbTextBrowser->isVisible())
    {
        emit onUpdateBreadcrumb();
    }
}

void MainWindow::onWordWrapChanged(bool enabled)
{
    wordWrapEnabled = enabled;

    // the wrap is a property of every document tab, not only of the current one
    const auto mode = enabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
            editor->setLineWrapMode(mode);
    }

    QSettings().setValue(QStringLiteral("view/wordWrap"), enabled);
}

void MainWindow::onAutoIndentChanged(bool enabled)
{
    autoIndentEnabled = enabled;

    // the setting is a property of every document tab, not only of the current one
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
            editor->setAutoIndentEnabled(enabled);
    }

    QSettings().setValue(QStringLiteral("view/autoIndent"), enabled);
}

void MainWindow::onEditorFontClicked()
{
    bool accepted = false;
    const QFont font = QFontDialog::getFont(&accepted, editorFont, this, tr("Editor font"));
    if (!accepted)
        return;

    editorFont = font;
    QSettings().setValue(QStringLiteral("editor/font"), font);

    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
            editor->setFont(font);
    }
}

void MainWindow::onStopWatchVisibilityChanged(bool visible)
{
    ui->stopwatchGroupBox->setVisible(visible);
}

void MainWindow::onConsoleVisibilityChanged(bool visible)
{
    if (visible)
    {
        if (!consoleDirectoryInitialized)
        {
            // starts where the article is (the commands see the file also as $STC_FILE), later the user decides with cd
            consoleDirectoryInitialized = true;
            const QString fileName = currentEditor()->getFileName();
            consoleWidget->setWorkingDirectory(fileName.isEmpty() ? lastDirectory : QFileInfo(fileName).absolutePath());
        }
        consoleWidget->focusInput();
    }
    else if (consoleWidget->isAncestorOf(QApplication::focusWidget()))
    {
        currentEditor()->setFocus();
    }
}

void MainWindow::onRightPanelVisibilityChanged(bool visible)
{
    ui->buttonsEmittingStc->setVisible(visible);
    ui->contextsTabWidget->setVisible(visible);
    ui->goToLineGroupBox->setVisible(visible);
    ui->stopwatchGroupBox->setVisible(visible);
}

void MainWindow::onViewMenuAboutToShow()
{
    /** this functions is because checkboxes are not visible when there are icons close to actions.
     *  That is why when we want to have both: checkboxes and icons we need to have checkbox as unicode character **/
    auto changeCheckedState = [](QAction* action)
    {
        if (action->isChecked())
        {
            auto newText = action->text().replace(u8"☐", u8"☑");
            action->setText(newText);
        }
        else
        {
            auto newText = action->text().replace(u8"☑", u8"☐");
            action->setText(newText);
        }
    };

    changeCheckedState(ui->actionBreadcrumb);
    changeCheckedState(ui->actionGo_to_line);
    changeCheckedState(ui->actionSTC_Tags_buttons);
    changeCheckedState(ui->actionShort_conspect);
    changeCheckedState(ui->actionRight_panel);
    changeCheckedState(ui->actionCpp0x_news);
    changeCheckedState(ui->actionStc_Preview_account_at_Cpp0x_pl_required);
    changeCheckedState(ui->actionStop_watch_uptime_and_working_time);
    changeCheckedState(ui->actionWord_wrap);
    changeCheckedState(ui->actionAuto_indent);
}

void MainWindow::surroundSelectedTextWithTag(QString divClass, QString textBase, QString extraAttributes, bool closable)
{
    auto cursor = currentEditor()->textCursor();
    QString selectedText = cursor.selectedText();

    const auto textOpening = "[" + textBase + extraAttributes + "]";
    const auto textEnding = closable ? "[/" + textBase + "]" : "";
    QString modifiedText = textOpening + selectedText + textEnding;

    putTextBackToCursorPosition(cursor, divClass, selectedText, textEnding, modifiedText);
}

void MainWindow::putTextBackToCursorPosition(QTextCursor &cursor, QString divClass,
                                             QString selectedText, QString textEnding, QString modifiedText)
{
    if (divClass.size())
    {
        QString safeModifiedText = modifiedText.toHtmlEscaped();

        QString html = "<div class=\"" + divClass + "\">"
                       + safeModifiedText + "</div>";
        cursor.insertHtml(html);
    }
    else
    {
        cursor.insertText(modifiedText);
    }

    if (0 == selectedText.size()) {
        auto currentPosition = cursor.position();
        auto positionMovedBeforeTextEnding = currentPosition - textEnding.size();
        cursor.setPosition(positionMovedBeforeTextEnding);
        currentEditor()->setTextCursor(cursor);
    }

    currentEditor()->setFocus();
}

void MainWindow::surroundSelectedTextWithAHrefTag()
{
    auto cursor = currentEditor()->textCursor();
    QString selectedText = cursor.selectedText();

    constexpr const char* beginOfTag = R"([a href=")";
    constexpr const char* endOfTag = R"("])";
    constexpr const char* attributeName = R"( name=")";

    if (0 == selectedText.size())
    {
        // Insert empty a tag with cursor positioned inside href attribute
        const QString modifiedText = QString(beginOfTag) + endOfTag;
        putTextBackToCursorPosition(cursor, "a", selectedText, "", modifiedText);
        // Position cursor inside href attribute
        int pos = cursor.position() - strlen(endOfTag);
        cursor.setPosition(pos);
        currentEditor()->setTextCursor(cursor);
        return;
    }

    auto [link, restOfText] = extractLink(selectedText);
    QString modifiedText = beginOfTag + link;
    if (restOfText.size())
    {
        modifiedText += QString('"') + attributeName + restOfText;
    }
    modifiedText += endOfTag;
    putTextBackToCursorPosition(cursor, "a", selectedText, "", modifiedText);
}

void MainWindow::surroundSelectedTextWithImgTag()
{
    auto cursor = currentEditor()->textCursor();
    QString selectedText = cursor.selectedText().trimmed();

    constexpr const char* beginOfTag = R"([img src=")";
    constexpr const char* endOfTag = R"("])";
    constexpr const char* opisAttr = R"(" opis=")";

    if (selectedText.isEmpty())
    {
        // No text selected — insert empty img tag with cursor positioned inside src attribute
        putTextBackToCursorPosition(cursor, "img", selectedText, "", QString(beginOfTag) + endOfTag);
        // Position cursor inside src attribute
        int pos = cursor.position() - strlen(endOfTag);
        cursor.setPosition(pos);
        currentEditor()->setTextCursor(cursor);
        return;
    }

    // Common image file extensions (extend as needed)
    static const QStringList imageExtensions = {
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg", ".avif", ".tiff"
    };

    // Split selected text into words
    const QStringList words = selectedText.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    QString detectedImage;
    int imageIndex = -1;

    // Look for any word that ends with a known image extension
    for (int i = 0; i < words.size(); ++i)
    {
        const QString& word = words[i];
        for (const auto& ext : imageExtensions)
        {
            if (word.endsWith(ext, Qt::CaseInsensitive))
            {
                detectedImage = word;
                imageIndex = i;
                break;
            }
        }
        if (!detectedImage.isEmpty())
            break;
    }

    QString result;

    if (!detectedImage.isEmpty())
    {
        // Found a likely image path in the selection
        QString description;

        if (words.size() == 1)
        {
            // Only one word selected and it's an image path — no description needed
            result = beginOfTag + detectedImage + endOfTag;
        }
        else
        {
            // Build description from remaining words
            QStringList copy = words;
            copy.removeAt(imageIndex);
            description = copy.join(' ');
            result = beginOfTag + detectedImage + opisAttr + description + endOfTag;
        }
    }
    else
    {
        // No image path detected — use full selection as description, leave src empty
        result = beginOfTag;
        result += R"(" opis=")" + selectedText + endOfTag;
    }

    putTextBackToCursorPosition(cursor, "img", selectedText, "", result);
}

void MainWindow::setDisabledMenuActionsDependingOnOpenedFile(bool disabled)
{
    ui->actionCopy_absolute_path->setDisabled(disabled);
    ui->actionCopy_basename->setDisabled(disabled);
    ui->actionReload_file->setDisabled(disabled);
    ui->actionRename_file->setDisabled(disabled);
}

void MainWindow::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(GeometryNames::GEOMETRY).toByteArray();
    const QByteArray windowState = settings.value(GeometryNames::SETTINGS_WINDOW_STATE).toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }
    if (!windowState.isEmpty())
    {
        restoreState(windowState);
    }

    lastDirectory = settings.value(GeometryNames::LAST_DIRECTORY, QDir::homePath()).toString();

    wordWrapEnabled = settings.value(QStringLiteral("view/wordWrap"), true).toBool();
    const auto wrapMode = wordWrapEnabled ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
            editor->setLineWrapMode(wrapMode);
    }
    QSignalBlocker wrapBlocker(ui->actionWord_wrap);
    ui->actionWord_wrap->setChecked(wordWrapEnabled);

    autoIndentEnabled = settings.value(QStringLiteral("view/autoIndent"), true).toBool();
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
            editor->setAutoIndentEnabled(autoIndentEnabled);
    }
    QSignalBlocker autoIndentBlocker(ui->actionAuto_indent);
    ui->actionAuto_indent->setChecked(autoIndentEnabled);

    const QFont savedFont = settings.value(QStringLiteral("editor/font")).value<QFont>();
    if (!savedFont.family().isEmpty())
    {
        editorFont = savedFont;
        for (int i = 0; i < ui->editorTabs->count(); ++i)
        {
            if (auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i)))
                editor->setFont(editorFont);
        }
    }

    ui->stcPreviewWidget->setScrollSyncEnabled(settings.value(GeometryNames::PREVIEW_SCROLL_SYNC, true).toBool());
    ui->stcPreviewWidget->setClickSyncEnabled(settings.value(GeometryNames::PREVIEW_CLICK_SYNC, true).toBool());

    QVariantMap filesMap = settings.value(GeometryNames::RECENT_FILES_LIST).toMap();
    for (auto it = filesMap.begin(); it != filesMap.end(); ++it)
    {
        RecentFileInfo info;
        QVariantMap fileInfo = it.value().toMap();
        info.cursorPosition = fileInfo["position"].toInt();
        info.scrollPosition = fileInfo["scrollPosition"].toInt();
        info.lastOpened = fileInfo["lastOpened"].toDateTime();
        if (!info.lastOpened.isValid())  // if no datetime
        {
            info.lastOpened = QDateTime::currentDateTime();
        }
        recentFilesWithPositions.insert(it.key(), info);
    }

    consoleWidget->setHistory(settings.value(GeometryNames::CONSOLE_HISTORY).toStringList());

    bool resumeSession = settings.value(GeometryNames::RESUME_SESSION, true).toBool();
    ui->actionResumeSession->setChecked(resumeSession);
}

void MainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue(GeometryNames::GEOMETRY, saveGeometry());
    settings.setValue(GeometryNames::SETTINGS_WINDOW_STATE, saveState());

    if (! lastDirectory.isEmpty())
    {
        settings.setValue(GeometryNames::LAST_DIRECTORY, lastDirectory);
    }

    QVariantMap recentFilesMap;
    for (auto it = recentFilesWithPositions.begin(); it != recentFilesWithPositions.end(); ++it)
    {
        QVariantMap fileInfo;
        fileInfo["position"] = it.value().cursorPosition;
        fileInfo["scrollPosition"] = it.value().scrollPosition;
        fileInfo["lastOpened"] = it.value().lastOpened;
        recentFilesMap.insert(it.key(), fileInfo);
    }
    if (! recentFilesMap.isEmpty())
    {
        settings.setValue(GeometryNames::RECENT_FILES_LIST, recentFilesMap);
    }

    // the documents open in the tabs, in tab order, and the one shown last
    QStringList openFiles;
    for (int i = 0; i < ui->editorTabs->count(); ++i)
    {
        const auto* editor = qobject_cast<CodeEditor*>(ui->editorTabs->widget(i));
        if (editor && !editor->getFileName().isEmpty())
            openFiles << editor->getFileName();
    }
    settings.setValue(GeometryNames::SESSION_OPEN_FILES, openFiles);

    if (currentEditor() && !currentEditor()->getFileName().isEmpty())
        settings.setValue(GeometryNames::SESSION_ACTIVE_FILE, currentEditor()->getFileName());
    else
        settings.remove(GeometryNames::SESSION_ACTIVE_FILE);

    settings.setValue(GeometryNames::RESUME_SESSION, ui->actionResumeSession->isChecked());
}

void MainWindow::updateRecentFiles(const QString& path)
{
    int cursorPos = currentEditor()->textCursor().position();
    int scrollPos = currentEditor()->verticalScrollBar()->value();

    recentFilesWithPositions.remove(path);
    RecentFileInfo info;
    info.cursorPosition = cursorPos;
    info.scrollPosition = scrollPos;
    info.lastOpened = QDateTime::currentDateTime();
    recentFilesWithPositions.insert(path, info);

    while (recentFilesWithPositions.size() > maxElementsInListOfLastElements)
    {
        recentFilesWithPositions.erase(--recentFilesWithPositions.end());
    }
}

void MainWindow::onResumeSessionToggled(bool checked)
{
    QSettings settings;
    settings.setValue(GeometryNames::RESUME_SESSION, checked);
}

void MainWindow::onLanguageChanged()
{
    QAction* action = qobject_cast<QAction*>(sender());
    
    QString language = "en"; // default
    
    if (action == ui->actionPolish) {
        language = "pl";
    } else if (action == ui->actionEnglish) {
        language = "en";
    }
    
    // Save language preference
    QSettings settings;
    settings.setValue("language", language);
    
    // Remove old translator if exists
    if (m_translator) {
        QApplication::instance()->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }
    
    // Load new translations
    m_translator = new QTranslator(this);
    
    QString translationFile = ":/translations/stc_editor_" + language + ".qm";
    
    if (m_translator->load(translationFile)) {
        QApplication::instance()->installTranslator(m_translator);
    } else {
        qWarning() << "Failed to load translation file for language:" << language;
        delete m_translator;
        m_translator = nullptr;
    }
    
    // Re-translate UI
    ui->retranslateUi(this);
    
    // Update dynamically created UI elements
    onRecentRecentFilesMenuOpened();
    consoleDockWidget->setWindowTitle(tr("Console"));
    consoleDockWidget->toggleViewAction()->setText(tr("Console"));
}

QList<std::pair<QAction*, SyntaxMode>> MainWindow::syntaxMenuActions() const
{
    return {
        { ui->actionSyntaxStc, SyntaxMode::Stc },
        { ui->actionSyntaxPlainText, SyntaxMode::PlainText },
        { ui->actionSyntaxCpp, SyntaxMode::Cpp },
        { ui->actionSyntaxPython, SyntaxMode::Python },
        { ui->actionSyntaxXml, SyntaxMode::Xml },
        { ui->actionSyntaxJson, SyntaxMode::Json },
    };
}

void MainWindow::onSyntaxModeChanged(SyntaxMode mode)
{
    for (const auto& [action, actionMode] : syntaxMenuActions())
        action->setChecked(actionMode == mode);

    // checking whether the STC tags are closed makes no sense when there are no STC tags
    ui->actioncheck_if_tags_are_closed->setEnabled(syntaxmode::usesStcMarkup(mode));
}

void MainWindow::setUpEncodingMenu()
{
    encodingMenu = new QMenu(tr("Encoding"), this);
    encodingCurrentAction = encodingMenu->addAction(QString());
    encodingCurrentAction->setEnabled(false);
    encodingMenu->addSeparator();
    saveEncodingMenu = encodingMenu->addMenu(tr("Save in encoding"));
    reloadEncodingMenu = encodingMenu->addMenu(tr("Reload with encoding"));
    encodingActions = new QActionGroup(this);
    encodingActions->setExclusive(true);
    menuBar()->insertMenu(ui->menuLanguage->menuAction(), encodingMenu);

    connect(encodingMenu, &QMenu::aboutToShow, this, &MainWindow::updateEncodingMenuHeader);
    connect(saveEncodingMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildSaveEncodingMenu);
    connect(reloadEncodingMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildReloadEncodingMenu);
}

QStringList MainWindow::offeredEncodings() const
{
    return { "UTF-8", "UTF-16LE", "UTF-16BE", "windows-1250", "ISO-8859-2", "windows-1252", "ISO-8859-15", "IBM852" };
}

void MainWindow::updateEncodingMenuHeader()
{
    if (!encodingCurrentAction)
        return; // the menu is built in connectSignals2Slots(), after the first tab already exists

    encodingCurrentAction->setText(tr("Current encoding: %1").arg(currentEditor()->saveEncoding()));
    // a new document has nothing on disk to read again
    reloadEncodingMenu->setEnabled(!currentEditor()->getFileName().isEmpty());
}

void MainWindow::rebuildSaveEncodingMenu()
{
    // the entries of the previous opening are deleted with the menu; the group forgets them by itself
    saveEncodingMenu->clear();

    const QString current = currentEditor()->saveEncoding();

    QStringList encodings = offeredEncodings();
    // the file may be in an encoding which is not on the list (e.g. one found by detection): it is shown too
    if (!std::any_of(encodings.cbegin(), encodings.cend(),
                     [&current](const QString& name) { return name.compare(current, Qt::CaseInsensitive) == 0; }))
    {
        encodings.prepend(current);
    }

    for (const QString& name : std::as_const(encodings))
    {
        QAction* action = saveEncodingMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(name.compare(current, Qt::CaseInsensitive) == 0);
        encodingActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, name] { onEncodingChosen(name); });
    }
}

void MainWindow::rebuildReloadEncodingMenu()
{
    reloadEncodingMenu->clear();
    for (const QString& name : offeredEncodings())
    {
        QAction* action = reloadEncodingMenu->addAction(name);
        connect(action, &QAction::triggered, this, [this, name] { onReloadWithEncodingChosen(name); });
    }
}

void MainWindow::onEncodingChosen(const QString& encodingName)
{
    const QString current = currentEditor()->saveEncoding();
    if (encodingName.compare(current, Qt::CaseInsensitive) == 0)
        return;

    const auto answer = QMessageBox::question(
        this,
        tr("Change encoding"),
        tr("Change the encoding of the file from %1 to %2?\n\nThe text is converted and the file is saved in %2 "
           "when you save it.")
            .arg(current, encodingName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (answer == QMessageBox::Yes)
        currentEditor()->setSaveEncoding(encodingName);
}

void MainWindow::onReloadWithEncodingChosen(const QString& encodingName)
{
    // unsaved changes are asked about (save, discard or cancel); without them nothing is asked
    if (!operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Reload the file?"),
            tr("Do You really want to read the file again in the encoding %1 and lose the unsaved changes?")
                .arg(encodingName),
            /*restoreEditorAfterwards=*/false))
    {
        return;
    }

    currentEditor()->reloadFileWithEncoding(encodingName);
}

void MainWindow::restoreLastSession()
{
    QSettings settings;
    if (!settings.value(GeometryNames::RESUME_SESSION, true).toBool())
        return;

    // every document which was open in a tab is reopened as a tab, in the same order
    const QStringList openFiles = settings.value(GeometryNames::SESSION_OPEN_FILES).toStringList();
    if (!openFiles.isEmpty())
    {
        const QString activeFile = settings.value(GeometryNames::SESSION_ACTIVE_FILE).toString();

        for (const QString& filePath : openFiles)
        {
            if (!QFile::exists(filePath))
                continue;

            openFileInTab(filePath);

            const auto info = recentFilesWithPositions.value(filePath);
            if (currentEditor() && info.cursorPosition > 0)
            {
                QTextCursor cursor = currentEditor()->textCursor();
                cursor.setPosition(info.cursorPosition);
                currentEditor()->setTextCursor(cursor);
                currentEditor()->ensureCursorVisible();

                if (info.scrollPosition > 0)
                    currentEditor()->verticalScrollBar()->setValue(info.scrollPosition);
            }
        }

        // the tab which was active last is shown again
        const int activeIndex = findTabIndexForFile(activeFile);
        if (activeIndex >= 0)
            ui->editorTabs->setCurrentIndex(activeIndex);

        return;
    }

    // no saved tab session (e.g. files were never saved with names): fall back to the most recently opened file
    if (recentFilesWithPositions.isEmpty())
        return;

    const auto sorted = getSortedExistingRecentFiles(recentFilesWithPositions);
    if (sorted.isEmpty())
        return;

    const auto &[filePath, info] = sorted.first();

    if (loadFileContentToEditorDistargingCurrentContent(filePath))
    {
        QTextCursor cursor = currentEditor()->textCursor();
        cursor.setPosition(info.cursorPosition);
        currentEditor()->setTextCursor(cursor);
        currentEditor()->ensureCursorVisible();

        if (info.scrollPosition > 0)
        {
            currentEditor()->verticalScrollBar()->setValue(info.scrollPosition);
        }
    }
}

void MainWindow::onFileContentChanged(const QString &fileName, int changedLines)
{
    if (changedLines)
    {
        const auto modificationInfo = currentEditor()->getFileModificationInfoText();
        updateWindowTitle(fileName, modificationInfo);
    }
    else
    {
        updateWindowTitle(fileName);
    }
}

void MainWindow::onUpdateBreadcrumb()
{
    if (!ui->breadcrumbTextBrowser->isHidden())
    {
        QTextCursor cursor = currentEditor()->textCursor();
        ui->breadcrumbTextBrowser->updateBreadcrumb(cursor);
    }
}

void MainWindow::onCopyPreviewHtmlRequested()
{
    ui->stcPreviewWidget->copyRenderedHtmlToClipboard();
}

void MainWindow::onEditorViewportMoved()
{
    if (previewClickInProgress)
    {
        return; // the user has just clicked the preview: it must not run away from under the mouse
    }

    const CodeEditor::ViewportTop top = currentEditor()->viewportTop();
    ui->stcPreviewWidget->scrollToSourceLine(top.line, top.fraction, top.atEndOfDocument);
}

void MainWindow::onPreviewClicked(int sourcePosition)
{
    // Moving the cursor can scroll the editor, and the editor would scroll the preview back (see onEditorViewportMoved)
    previewClickInProgress = true;
    currentEditor()->showPosition(sourcePosition);
    currentEditor()->setFocus();
    QTimer::singleShot(0, this, [this] { previewClickInProgress = false; });
}

void MainWindow::onShowStcPreviewTriggered()
{
    ui->stcPreviewDockWidget->raise();

    if (ui->stcPreviewDockWidget->isHidden())
    {
        return;
    }

    if (ui->stcPreviewWidget->isPreviewInitialized())
    {
        return;
    }

    QSettings settings;
    QString savedUser = settings.value(GeometryNames::LOGIN_USERNAME).toString();
    QString savedPass = settings.value(GeometryNames::LOGIN_PASSWORD).toString();
    bool savedRemember = settings.value(GeometryNames::LOGIN_REMEMBER, false).toBool();
    bool savedAutoLogin = settings.value(GeometryNames::LOGIN_AUTO_LOGIN, false).toBool();

    // Auto-login if enabled and credentials are saved
    if (savedAutoLogin && savedRemember && !savedUser.isEmpty())
    {
        ui->stcPreviewWidget->login(savedUser, savedPass);
    }
    else
    {
        LoginDialog dlg(this);

        // Pre-fill saved credentials if available
        if (!savedUser.isEmpty())
        {
            dlg.setCredentials(savedUser, savedPass);
            dlg.setRememberChecked(savedRemember);
            dlg.setAutoLoginChecked(savedAutoLogin);
        }

        if (dlg.exec() != QDialog::Accepted)
            return;

        // Save credentials if "Remember login" is checked
        if (dlg.isRememberChecked())
        {
            settings.setValue(GeometryNames::LOGIN_USERNAME, dlg.username());
            settings.setValue(GeometryNames::LOGIN_PASSWORD, dlg.password());
            settings.setValue(GeometryNames::LOGIN_REMEMBER, true);
            settings.setValue(GeometryNames::LOGIN_AUTO_LOGIN, dlg.isAutoLoginChecked());
        }
        else
        {
            settings.remove(GeometryNames::LOGIN_USERNAME);
            settings.remove(GeometryNames::LOGIN_PASSWORD);
            settings.remove(GeometryNames::LOGIN_REMEMBER);
            settings.remove(GeometryNames::LOGIN_AUTO_LOGIN);
        }

        ui->stcPreviewWidget->login(dlg.username(), dlg.password());
    }

    // the text of every document tab refreshes the preview (see connectEditorSignals)
    ui->stcPreviewWidget->updateText(currentEditor()->toPlainText());

    connect(ui->stcPreviewWidget, &StcPreviewWidget::loginFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Login error"), msg);
    });

    connect(ui->stcPreviewWidget, &StcPreviewWidget::loginSucceeded, this, [this]() {
        ui->stcPreviewWidget->updateText(currentEditor()->toPlainText());
    });
}
