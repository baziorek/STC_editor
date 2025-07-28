#include <QKeyEvent>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDesktopServices>
#include <QClipboard>
#include <QStack>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "ui/shortcutsdialog.h"
#include "ui/stctagsbuttons.h"
#include "checkers/PairedTagsChecker.h"
#include "errorlist.h"
#include "types/documentstatistics.h"
#include "widgets/LoginDialog.h"
#include "widgets/DiffReviewDialog.h"
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


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{    
    ui->setupUi(this);

    loadSettings();

    setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/true);

    ui->stcPreviewDockWidget->hide();

    ui->findWidget->hide();
    ui->findDockWidget->hide();
    ui->findWidget->setCodeEditor(ui->textEditor);
    ui->textEditor->setFocus();

    ui->contextTableWidget->setTextEditor(ui->textEditor);
    ui->codesListTableWidget->setTextEditor(ui->textEditor);
    ui->todosTableWidget->setTextEditor(ui->textEditor);

    connectSignals2Slots();
    connectShortcutsFromCodeWidget();
    connectShortcuts();
}

void MainWindow::connectSignals2Slots()
{
    connect(ui->buttonsEmittingStc, &StcTagsButtons::buttonPressed, this, &MainWindow::onStcTagsButtonPressed);
    connect(ui->contextTableWidget, &FilteredTagTableWidget::goToLineClicked, ui->textEditor, &CodeEditor::go2LineRequested);
    connect(ui->goToLineGroupBox, &GoToLineWidget::onGoToLineRequested, ui->textEditor, &CodeEditor::go2LineRequested);
    connect(ui->findWidget, &FindDialog::jumpToLocationRequested, ui->textEditor, &CodeEditor::goToLineAndOffset);
    connect(ui->textEditor, &CodeEditor::totalLinesCountChanged, ui->goToLineGroupBox, &GoToLineWidget::setMaxLine);
    connect(ui->textEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateBreadcrumb);
    connect(ui->textEditor, &CodeEditor::numberOfModifiedLinesChanged, [this](int linesNumber) {
        this->onFileContentChanged(ui->textEditor->getFileName(), linesNumber);
    });
    connect(ui->todosTableWidget, &TodoTrackerTableWidget::todosTotalCountChanged, [this](int todosTotal) {
        ui->contextsTabWidget->setTabText(2, tr("TODOs (") + QString::number(todosTotal) + ")");
    });

    ui->breadcrumbTextBrowser->setTextEditor(ui->textEditor);
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
    auto cursor = ui->textEditor->textCursor();
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

    // 3. Default: Surround selection with tags
    QString result = openTag + trimmedText + closeTag;
    result = QString(selectedText.left(leadingSpaces)) + result + QString(selectedText.right(trailingSpaces));
    cursor.insertText(result);
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

        updateRecentFiles(filePath);
        onRecentRecentFilesMenuOpened();
        loadFileContentToEditorDistargingCurrentContent(filePath);

        const int position = recentFilesWithPositions.value(filePath).cursorPosition;
        QTextCursor cursor = ui->textEditor->textCursor();
        cursor.setPosition(position);
        ui->textEditor->setTextCursor(cursor);
        ui->textEditor->ensureCursorVisible();
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
    const auto fileName = ui->textEditor->getFileName();
    if (! fileName.isEmpty())
    {
        const auto fileNameAbsolutePath = QFileInfo(fileName).absoluteFilePath();
        QApplication::clipboard()->setText(fileNameAbsolutePath);
    }
}

void MainWindow::onCopyFileBaseNamePressed()
{
    const auto fileName = ui->textEditor->getFileName();
    if (! fileName.isEmpty())
    {
        QString baseName = QFileInfo(fileName).fileName();
        QApplication::clipboard()->setText(baseName);
    }
}

void MainWindow::onOpenParentDirectoryPressed()
{
    QString fileName = ui->textEditor->getFileName();
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

    dialog->addShortcuts(ui->textEditor->listOfShortcuts(), "Editor");

    QList<QAction*> menuActions = findChildren<QAction*>();
    dialog->addQActions(menuActions, "Application Menu");

    dialog->adjustSizeToContents();

    dialog->exec();
}

void MainWindow::onFileStatsRequested()
{
    auto result = DocumentStatistics::analyze(ui->textEditor);
    QMessageBox::information(this, "File statistics", result.toQString());
}

void MainWindow::onFindTriggered(bool)
{
    QWidget* focusWidget = QApplication::focusWidget();

    bool isFindVisible = ui->findDockWidget->isVisible();
    bool focusInEditor = (focusWidget == ui->textEditor);
    bool focusInFind = ui->findWidget->isAncestorOf(focusWidget);

    if (focusInEditor)
    {
        // Show the find widget and focus the input field
        if (!isFindVisible)
            ui->findDockWidget->setVisible(true);

        ui->findWidget->setVisible(true);
        ui->findWidget->focusInput();
    }
    else if (focusInFind)
    {
        // Hide the find widget and return focus to the editor
        ui->findDockWidget->setVisible(false);
        ui->textEditor->setFocus();
    }
    else
    {
        // Fallback case: toggle visibility and set focus accordingly
        ui->findDockWidget->setVisible(!isFindVisible);

        if (!isFindVisible)
            ui->findWidget->focusInput();
        else
            ui->textEditor->setFocus();
    }

    // Update the action's checked state without retriggering the slot
    QSignalBlocker blocker(ui->actionFind);
    ui->actionFind->setChecked(ui->findDockWidget->isVisible());

    /// checkbox as unicode
    auto [text2Change, newText] = make_pair(u8"☑", u8"☐");
    if (ui->actionFind->isChecked())
        std::tie(text2Change, newText) = make_pair(u8"☐", u8"☑");
    auto newActionText = ui->actionFind->text().replace(text2Change, newText);
    ui->actionFind->setText(newActionText);
}

[[deprecated("Instead of them mnemoniks from Qt are being used")]] void MainWindow::connectShortcutsFromCodeWidget()
{
    connect(ui->textEditor, &CodeEditor::shortcutPressed_bold, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::BOLD], tagsClasses[StcTags::BOLD]);
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_run, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::RUN], tagsClasses[StcTags::RUN]);
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_warning, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_WARNING], tagsClasses[StcTags::DIV], R"( class="uwaga")");
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_tip, [this]() {
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::DIV_TIP], tagsClasses[StcTags::DIV], R"( class="tip")");
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_href, [this]() {
        this->surroundSelectedTextWithAHrefTag();
    });

    connect(ui->textEditor, &CodeEditor::shortcutPressed_h1, [this] {
        this->surroundSelectedTextWithTag("h1", "h1");
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_h2, [this] {
        this->surroundSelectedTextWithTag("h2", "h2");
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_h3, [this] {
        this->surroundSelectedTextWithTag("h3", "h3");
    });
    connect(ui->textEditor, &CodeEditor::shortcutPressed_h4, [this] {
        this->surroundSelectedTextWithTag("h4", "h4");
    });
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
        closeApplicationReturningIfClosed();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Close without saving?"), tr("Do You really want to close the editor without saving unsaved changes?")))
    {
        QWidget::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::operationWhichDiscardsChangesRequestedReturningIfDiscarded(const QString &dialogTitle, const QString &dialogMessage)
{
    if (!ui->textEditor->noUnsavedChanges())
    {
        DiffReviewDialog dialog(ui->textEditor, dialogTitle, dialogMessage, this);
        if (dialog.exec() == QDialog::Accepted)
        {
            switch (dialog.userChoice())
            {
            case DiffReviewDialog::Save:
                if (onSavePressed())
                {
                    ui->textEditor->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
                    return true;
                }
                else // saving failed
                {
                    return false;
                }
                break;
            case DiffReviewDialog::Discard:
                ui->textEditor->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
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
    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Close without saving?"), tr("Do You really want to close the editor without saving unsaved changes?")))
    {
        close();
        return true;
    }
    return false;
}

void MainWindow::onNewFilePressed()
{
    ui->todosTableWidget->clearTodos();
    ui->contextTableWidget->clear();

    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Confirm discarding current file content?"), tr("Do You really want to lose currently unsaved changes and create new file")))
    {
        ui->textEditor->newEmptyFile();
        updateWindowTitle();
        setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/true);
    }
}

bool MainWindow::onSaveAsPressed()
{
    const auto fileName = chooseFileWithDialog(QFileDialog::AcceptSave);
    return saveEntireContent2File(fileName);
}

QString MainWindow::chooseFileWithDialog(QFileDialog::AcceptMode acceptMode)
{
    QFileDialog dialog(this);
    QStringList nameFilters;
    nameFilters << QWidget::tr("Text file (*.txt)");
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
        if (ui->textEditor->saveEntireContent2File(fileName))
        {
            setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/false);
            return true;
        }
    }
    return false;
}

void MainWindow::updateWindowTitle(QString fileName, QString suffix)
{
    if (fileName.isEmpty())
    {
        setWindowTitle(qApp->applicationName());
    }
    else
    {
        auto newFileName = qApp->applicationName() + ": " + fileName;
        if (! suffix.isEmpty())
        {
            newFileName += " \t[" + suffix + ']';
        }
        setWindowTitle(newFileName);
    }
}

bool MainWindow::onSavePressed()
{
    auto outputFileName = ui->textEditor->getFileName();
    if (outputFileName.isEmpty())
    {
        return onSaveAsPressed();
    }
    else if (ui->textEditor->noUnsavedChanges())
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
    if (! operationWhichDiscardsChangesRequestedReturningIfDiscarded(
            tr("Decline currently unsaved changes?"), tr("Do You really want to decline currently unsaved changes and open new file?")))
    {
        return;
    }

    const auto fileName = chooseFileWithDialog(QFileDialog::AcceptOpen).trimmed();
    if (! fileName.isEmpty())
    {
        ui->todosTableWidget->clearTodos();
        ui->contextTableWidget->clear();

        loadFileContentToEditorDistargingCurrentContent(fileName);
    }
}

void MainWindow::onReloadFilePressed()
{
    if (ui->textEditor->getFileName().isEmpty())
    {
        return;
    }

    if (! operationWhichDiscardsChangesRequestedReturningIfDiscarded(
        tr("Confirm reloading and loosing unsaved content"), tr("Do You really want to reload the currently opened file?\n"
                                                                "Currently unsaved changes would be declined.")))
    {
        return;
    }

    ui->textEditor->reloadFromFile(/*discardChanges=*/true);
}

bool MainWindow::loadFileContentToEditorDistargingCurrentContent(const QString& fileName)
{
    ui->todosTableWidget->clearTodos();

    if (ui->textEditor->loadFileContentDistargingCurrentContent(fileName))
    {
        setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/false);

        updateWindowTitle(fileName);

        ui->actionReload_file->setEnabled(true);
        updateRecentFiles(fileName);

        ui->contextTableWidget->rebuildAllHeaders();

        return true;
    }

    QMessageBox::warning(this, "Error opening file", "File '" + fileName + "' failed to be opened from commandline!");

    return false;
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
    const auto text = ui->textEditor->toPlainText().toStdString();
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
    changeCheckedState(ui->actionStc_Preview_account_at_Cpp0x_pl_required);
}

void MainWindow::surroundSelectedTextWithTag(QString divClass, QString textBase, QString extraAttributes, bool closable)
{
    auto cursor = ui->textEditor->textCursor();
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
        ui->textEditor->setTextCursor(cursor);
    }

    ui->textEditor->setFocus();
}

void MainWindow::surroundSelectedTextWithAHrefTag()
{
    auto cursor = ui->textEditor->textCursor();
    QString selectedText = cursor.selectedText();

    constexpr const char* beginOfTag = R"([a href=")";
    constexpr const char* endOfTag = R"("])";
    constexpr const char* attributeName = R"( name=")";

    if (0 == selectedText.size())
    {
        const QString modifiedText = QString(beginOfTag) + '"' + attributeName + endOfTag;
        putTextBackToCursorPosition(cursor, "a", selectedText, "", modifiedText);
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
    auto cursor = ui->textEditor->textCursor();
    QString selectedText = cursor.selectedText().trimmed();

    constexpr const char* beginOfTag = R"([img src=")";
    constexpr const char* endOfTag = R"("])";
    constexpr const char* opisAttr = R"(" opis=")";

    if (selectedText.isEmpty())
    {
        // No text selected — insert empty img tag
        putTextBackToCursorPosition(cursor, "img", selectedText, "", QString(beginOfTag) + endOfTag);
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

    QVariantMap filesMap = settings.value(GeometryNames::RECENT_FILES_LIST).toMap();
    for (auto it = filesMap.begin(); it != filesMap.end(); ++it)
    {
        RecentFileInfo info;
        QVariantMap fileInfo = it.value().toMap();
        info.cursorPosition = fileInfo["position"].toInt();
        info.lastOpened = fileInfo["lastOpened"].toDateTime();
        if (!info.lastOpened.isValid())  // if no datetime
        {
            info.lastOpened = QDateTime::currentDateTime();
        }
        recentFilesWithPositions.insert(it.key(), info);
    }
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
        fileInfo["lastOpened"] = it.value().lastOpened;
        recentFilesMap.insert(it.key(), fileInfo);
    }
    if (! recentFilesMap.isEmpty())
    {
        settings.setValue(GeometryNames::RECENT_FILES_LIST, recentFilesMap);
    }
}

void MainWindow::updateRecentFiles(const QString& path)
{
    int cursorPos = ui->textEditor->textCursor().position();

    recentFilesWithPositions.remove(path);
    RecentFileInfo info;
    info.cursorPosition = cursorPos;
    info.lastOpened = QDateTime::currentDateTime();
    recentFilesWithPositions.insert(path, info);

    while (recentFilesWithPositions.size() > maxElementsInListOfLastElements)
    {
        recentFilesWithPositions.erase(--recentFilesWithPositions.end());
    }
}

void MainWindow::onFileContentChanged(const QString &fileName, int changedLines)
{
    if (changedLines)
    {
        const auto modificationInfo = ui->textEditor->getFileModificationInfoText();
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
        QTextCursor cursor = ui->textEditor->textCursor();
        ui->breadcrumbTextBrowser->updateBreadcrumb(cursor);
    }
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

    LoginDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    ui->stcPreviewWidget->login(dlg.username(), dlg.password());

    connect(ui->textEditor, &CodeEditor::textChanged, [this]() {
        if (ui->stcPreviewWidget->isVisible())
        {
            ui->stcPreviewWidget->updateText(ui->textEditor->toPlainText());
        }
    });

    connect(ui->stcPreviewWidget, &StcPreviewWidget::loginFailed, this, [this](const QString &msg) {
        QMessageBox::warning(this, "Login error", msg);
    });

    connect(ui->stcPreviewWidget, &StcPreviewWidget::loginSucceeded, this, [this]() {
        connect(ui->textEditor, &CodeEditor::textChanged, this, [this]() {
            ui->stcPreviewWidget->updateText(ui->textEditor->toPlainText());
        });
        ui->stcPreviewWidget->updateText(ui->textEditor->toPlainText());
    });
}
