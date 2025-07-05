#include <QKeyEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QRegularExpression>
#include <QMessageBox>
#include <QSettings>
#include <QDesktopServices>
#include <QClipboard>
#include <QStack>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "checkers/PairedTagsChecker.h"
#include "errorlist.h"
#include "ui/shortcutsdialog.h"
#include "ui/stctagsbuttons.h"
#include "types/documentstatistics.h"
using namespace std;

namespace
{
struct TextInsideTags
{
    QString tag, text;
};

std::pair<QString, QString> extractLink(const QString& text) {
    static QRegularExpression regex("(https?://\\S+)");
    QRegularExpressionMatch match = regex.match(text);

    if (match.hasMatch()) {
        QString link = match.captured(1);
        QString restOfText = text.mid(match.capturedEnd());
        return std::make_pair(link, restOfText);
    }

    /// If no link is found, return just text
    return std::make_pair("", text);
}

std::map<int, TextInsideTags> findTagMatches(const QRegularExpression& regex, const QString& text)
{
    std::map<int, TextInsideTags> textPerLine;
    for (QRegularExpressionMatchIterator matches = regex.globalMatch(text); matches.hasNext(); )
    {
        // TODO: Can we make finding line number more optimal?
        QRegularExpressionMatch match = matches.next();
        auto lineNumber = text.left(match.capturedStart(0)).count('\n') + 1;
        textPerLine.insert({lineNumber, TextInsideTags{match.captured(1), match.captured(2)}});
    }
    return textPerLine;
}
} // namespace


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{    
    ui->setupUi(this);

    loadSettings();

    setDisabledMenuActionsDependingOnOpenedFile();

    ui->findWidget->hide();
    ui->findDockWidget->hide();
    ui->findWidget->setCodeEditor(ui->textEditor);
    ui->textEditor->setFocus();

    connect(ui->buttonsEmittingStc, &StcTagsButtons::buttonPressed, this, &MainWindow::onStcTagsButtonPressed);
    connect(ui->contextTableWidget, &FilteredTagTableWidget::goToLineClicked, ui->textEditor, &CodeEditor::go2LineRequested);
    connect(ui->textEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateContextRequested);
    connect(ui->textEditor, &CodeEditor::totalLinesCountChanged, ui->goToLineGroupBox, &GoToLineWidget::setMaxLine);
    connect(ui->goToLineGroupBox, &GoToLineWidget::onGoToLineRequested, ui->textEditor, &CodeEditor::go2LineRequested);
    connect(ui->findWidget, &FindDialog::jumpToLocationRequested, ui->textEditor, &CodeEditor::goToLineAndOffset);
    connect(ui->textEditor, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateBreadcrumb);

    connectShortcutsFromCodeWidget();
    connectShortcuts();
}

void MainWindow::onStcTagsButtonPressed(StcTags stcTag)
{
    switch (stcTag)
    {
    case StcTags::RUN:
        surroundSelectedTextWithTag(tagsClasses[StcTags::RUN], tagsClasses[StcTags::RUN]);
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
    case StcTags::PKT:
        surroundSelectedTextWithTag(tagsClasses[StcTags::PKT], tagsClasses[StcTags::PKT], " ext");
        break;
    case StcTags::CSV:
        this->surroundSelectedTextWithTag(tagsClasses[StcTags::CSV], tagsClasses[StcTags::CSV], " extended header");
        break;
    case StcTags::BOLD:
        surroundSelectedTextWithTag(tagsClasses[StcTags::BOLD], tagsClasses[StcTags::BOLD]);
        break;
    case StcTags::ITALIC:
        surroundSelectedTextWithTag(tagsClasses[StcTags::ITALIC], tagsClasses[StcTags::ITALIC]);
        break;
    case StcTags::UNDERLINED:
        surroundSelectedTextWithTag(tagsClasses[StcTags::UNDERLINED], tagsClasses[StcTags::UNDERLINED]);
        break;
    case StcTags::STRUCK_OUT:
        surroundSelectedTextWithTag(tagsClasses[StcTags::STRUCK_OUT], tagsClasses[StcTags::STRUCK_OUT]);
        break;
    case StcTags::H1:
        surroundSelectedTextWithTag(tagsClasses[StcTags::H1], tagsClasses[StcTags::H1]);
        break;
    case StcTags::H2:
        surroundSelectedTextWithTag(tagsClasses[StcTags::H2], tagsClasses[StcTags::H2]);
        break;
    case StcTags::H3:
        surroundSelectedTextWithTag(tagsClasses[StcTags::H3], tagsClasses[StcTags::H3]);
        break;
    case StcTags::H4:
        surroundSelectedTextWithTag(tagsClasses[StcTags::H4], tagsClasses[StcTags::H4]);
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

void MainWindow::onRecentRecentFilesMenuOpened()
{
    ui->menuOpen_recent->clear();

    int shown = 0;
    for (const QString& filePath : recentFilesWithPositions.keys())
    {
        if (! QFile::exists(filePath))
            continue;

        const QString fileName = QFileInfo(filePath).fileName();
        QAction* recentAction = new QAction(fileName, ui->menuOpen_recent);
        recentAction->setData(filePath);
        recentAction->setToolTip(filePath);

        connect(recentAction, &QAction::triggered, this, [this, filePath]() {
            if (!QFile::exists(filePath)) {
                QMessageBox::warning(this, "File not found", "Plik nie istnieje:\n" + filePath);
                return;
            }
            updateRecentFiles(filePath);
            onRecentRecentFilesMenuOpened();
            loadFileContentToEditorDistargingCurrentContent(filePath);

            int pos = recentFilesWithPositions.value(filePath, 0);
            QTextCursor cursor = ui->textEditor->textCursor();
            cursor.setPosition(pos);
            ui->textEditor->setTextCursor(cursor);
            ui->textEditor->ensureCursorVisible();
        });

        ui->menuOpen_recent->addAction(recentAction);

        if (++shown >= 5)
            break;
    }

    if (shown == 0) {
        QAction* emptyAction = new QAction(tr("No recent files"), this);
        emptyAction->setEnabled(false);
        ui->menuOpen_recent->addAction(emptyAction);
    }
    else if (shown > 0) {
        ui->menuOpen_recent->addSeparator();

        QAction* clearAction = new QAction(tr("Clear Recent Files"), ui->menuOpen_recent);
        connect(clearAction, &QAction::triggered, this, [this]() {
            recentFilesWithPositions.clear();
            onRecentRecentFilesMenuOpened();
        });
        ui->menuOpen_recent->addAction(clearAction);
    }
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

void MainWindow::onFindTriggered(bool checked)
{
    ui->findWidget->setVisible(checked);
    ui->findDockWidget->setVisible(checked);

    if (checked)
    {
        ui->findWidget->focusInput();
    }
    else
    {
        ui->textEditor->setFocus();
    }
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
    focusGo2LineWidgetAction->setShortcut(QKeySequence("Ctrl+L"));
    addAction(focusGo2LineWidgetAction);
    connect(focusGo2LineWidgetAction, &QAction::triggered, [this]() {
        ui->goToLineGroupBox->setFocus();
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
    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded())
    {
        QWidget::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::operationWhichDiscardsChangesRequestedReturningIfDiscarded()
{
    if (!ui->textEditor->noUnsavedChanges())
    {
        QMessageBox::StandardButton reply =
            QMessageBox::question(this,
                                  "Close without saving?",
                                  "Do You really want to exit without saving unsaved changes?",
                                  QMessageBox::Discard|QMessageBox::No|QMessageBox::Save);
        if (reply == QMessageBox::Save)
        {
            if (! onSavePressed())
            {
                /// the state needs to be restored to prevend asking again
                ui->textEditor->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
                return false;
            }
        }
        else if (reply == QMessageBox::No)
        {
            return false;
        }
        else if (reply == QMessageBox::Discard)
        {
            /// the state needs to be restored to prevend asking again
            ui->textEditor->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
        }
    }
    return true;
}

bool MainWindow::closeApplicationReturningIfClosed()
{
    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded())
    {
        close();
        return true;
    }
    return false;
}

void MainWindow::onUpdateContextRequested()
{
    if (ui->contextTableWidget->isHidden())
    {
        return;
    }

    static QRegularExpression hRegex("\\[(h[1-6])\\](.*?)\\[/\\1\\]");
    static QRegularExpression divRegex("\\[(div)(?:\\s+[^\\]]+)?\\](.*?)\\[/\\1\\]", QRegularExpression::DotMatchesEverythingOption);

    const auto text = ui->textEditor->toPlainText();

    auto taggedTextLinePositions = findTagMatches(hRegex, text);
    taggedTextLinePositions.merge(findTagMatches(divRegex, text));

    updateContextTable(taggedTextLinePositions);
}

void MainWindow::updateContextTable(auto taggedTextLinePositions)
{
    ui->contextTableWidget->setRowCount(taggedTextLinePositions.size());

    unsigned rowNumber = 0;
    for (const auto& [lineNumber, tagAndText] : taggedTextLinePositions)
    {
        const auto& [tag, text] = tagAndText;
        ui->contextTableWidget->insertRow(rowNumber,
                                          /*lineNumber=*/lineNumber,
                                          /*tagName=*/tag,
                                          /*tagText=*/text);

        ++rowNumber;
    }
}

void MainWindow::onNewFilePressed()
{
    if (operationWhichDiscardsChangesRequestedReturningIfDiscarded())
    {
        ui->textEditor->clear();
        updateWindowTitle();
        ui->textEditor->setFileName("");
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
        QFile outputFile(fileName);
        outputFile.open(QIODeviceBase::WriteOnly);
        ui->textEditor->setFileName(fileName);
        return outputFile.write(ui->textEditor->toPlainText().toUtf8()) > -1;
    }
    return false;
}

void MainWindow::updateWindowTitle(QString fileName)
{
    if (fileName.isEmpty())
    {
        setWindowTitle(qApp->applicationName());
    }
    else
    {
        auto newFileName = qApp->applicationName() + ": " + fileName;
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
    if (! operationWhichDiscardsChangesRequestedReturningIfDiscarded())
    {
        return;
    }

    const auto fileName = chooseFileWithDialog(QFileDialog::AcceptOpen).trimmed();
    if (loadFileContentToEditorDistargingCurrentContent(fileName))
    {
        updateRecentFiles(fileName);
    }
}
bool MainWindow::loadFileContentToEditorDistargingCurrentContent(QString fileName)
{
    if (ui->textEditor->loadFileContentDistargingCurrentContent(fileName))
    {
        ui->contextTableWidget->clearTags();

        setDisabledMenuActionsDependingOnOpenedFile(/*disabled=*/false);

        updateWindowTitle(fileName);

        return true;
    }
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
    for (const auto [lineNumber, errorText] : tagsErrors)
    {
        ui->errorsInText->addError(lineNumber, QString::fromStdString(errorText));
    }
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

void MainWindow::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value("geometry").toByteArray();
    const QByteArray windowState = settings.value("windowState").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    if (!windowState.isEmpty())
        restoreState(windowState);

    lastDirectory = settings.value("lastDirectory", QDir::homePath()).toString();

    QVariantMap filesMap = settings.value("recentFiles").toMap();
    for (auto it = filesMap.begin(); it != filesMap.end(); ++it)
    {
        recentFilesWithPositions.insert(it.key(), it.value().toInt());
    }
}

void MainWindow::setDisabledMenuActionsDependingOnOpenedFile(bool disabled)
{
    ui->actionCopy_absolute_path->setDisabled(disabled);
    ui->actionCopy_basename->setDisabled(disabled);
}

void MainWindow::saveSettings()
{
    QSettings settings;

    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    settings.setValue("lastDirectory", lastDirectory);

    QVariantMap recentFilesMap;
    for (auto it = recentFilesWithPositions.begin(); it != recentFilesWithPositions.end(); ++it)
    {
        recentFilesMap.insert(it.key(), it.value());
    }
    settings.setValue("recentFiles", recentFilesMap);
}

void MainWindow::updateRecentFiles(const QString& path)
{
    constexpr int maxElementsInListOfLastElements = 5;

    int cursorPos = ui->textEditor->textCursor().position();

    recentFilesWithPositions.remove(path);
    recentFilesWithPositions.insert(path, cursorPos);

    while (recentFilesWithPositions.size() > maxElementsInListOfLastElements)
    {
        recentFilesWithPositions.erase(--recentFilesWithPositions.end());
    }
}

void MainWindow::onUpdateBreadcrumb()
{
    QTextCursor cursor = ui->textEditor->textCursor();
    int cursorPos = cursor.position();

    QString fullText = ui->textEditor->toPlainText();
    QString breadcrumbPath = getCurrentStcContextPath(fullText, cursorPos);

    ui->breadcrumbLabel->setText(breadcrumbPath);
}

QString MainWindow::getCurrentStcContextPath(const QString& text, int cursorPos)
{
    static const QSet<QString> ignorableTags = { "img", "a" };
    QStack<QString> contextStack;

    // 1. Main structure of headers
    static QRegularExpression headerRegex(R"(\[(h[1-6])\](.*?)\[/\1\])",
                                          QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    QMap<int, QString> headerLevels;  // key: level hN, value: text
    QRegularExpressionMatchIterator it = headerRegex.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        if (m.capturedStart() >= cursorPos)
            break;

        QString levelStr = m.captured(1);     // e.g. h1
        QString content = m.captured(2).trimmed();
        int level = levelStr.mid(1).toInt();  // only letter from h1–h6

        // Remove deeper levels
        for (auto l : headerLevels.keys())
        {
            if (l >= level)
                headerLevels.remove(l);
        }
        headerLevels[level] = levelStr + ": " + content;
    }

    // 2. Dynamic opened tags
    static QRegularExpression tagOpenRegex(R"(\[([a-z0-9]+)(?:\s+[^\]]+)?\])", QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression tagCloseRegex(R"(\[/([a-z0-9]+)\])", QRegularExpression::CaseInsensitiveOption);

    int index = 0;
    while (index < cursorPos)
    {
        QRegularExpressionMatch openMatch = tagOpenRegex.match(text, index);
        QRegularExpressionMatch closeMatch = tagCloseRegex.match(text, index);

        int openPos = openMatch.hasMatch() ? openMatch.capturedStart() : -1;
        int closePos = closeMatch.hasMatch() ? closeMatch.capturedStart() : -1;

        if (openPos != -1 && (closePos == -1 || openPos < closePos) && openPos < cursorPos) {
            QString tagName = openMatch.captured(1);
            if (!ignorableTags.contains(tagName) && !tagName.startsWith("h"))
            {
                contextStack.push(tagName);
            }
            index = openMatch.capturedEnd();
        }
        else if (closePos != -1 && closePos < cursorPos)
        {
            QString tagName = closeMatch.captured(1);
            if (!tagName.startsWith("h"))
            {
                int i = contextStack.size() - 1;
                while (i >= 0 && contextStack[i] != tagName)
                    --i;
                if (i >= 0)
                    contextStack.remove(i);
            }
            index = closeMatch.capturedEnd();
        }
        else
        {
            break;
        }
    }

    // 3. Build full path:
    QStringList breadcrumb;

    const QList<int> sortedKeys = headerLevels.keys();
    for (int k : sortedKeys)
        breadcrumb << headerLevels[k];

    breadcrumb.append(contextStack.toList());

    return breadcrumb.join(" > ");
}
