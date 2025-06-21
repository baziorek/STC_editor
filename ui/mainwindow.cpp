#include <cstdint>
#include <QKeyEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QRegularExpression>
#include <QMessageBox>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "checkers/PairedTagsChecker.h"
#include "errorlist.h"
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

void insertText2Cell(QTableWidget* table, int row, int column, const QString& text)
{
    if (auto* cell = table->item(row, column); cell == nullptr)
    {
        cell = new QTableWidgetItem(text);
        cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, column, cell);
    }
    else
    {
        table->item(row, column)->setText(text);
    }
};

QString chooseFileWithDialog(QWidget* parent, QFileDialog::AcceptMode acceptMode)
{
    QFileDialog dialog(parent);
    QStringList nameFilters;
    nameFilters << QWidget::tr("Text file (*.txt)");
    dialog.setNameFilters(nameFilters);
    dialog.setAcceptMode(acceptMode);
    dialog.setDefaultSuffix(".txt");

    if (dialog.exec())
    {
        const QString fileName = dialog.selectedFiles()[0];
        qDebug() << "Selected: " << fileName.toStdString();
        return fileName;
    }
    return {};
}
} // namespace


enum class StdTags: uint8_t
{
    RUN,
    CPP,
    PY,
    CODE,
    DIV,
    DIV_WARNING,
    DIV_TIP,
    A_HREF,
    PKT,
    BOLD,
    QUOTE
};

std::map<StdTags, QString> tagsClasses =
{
    make_pair(StdTags::RUN, "run"),
    make_pair(StdTags::CPP, "cpp"),
    make_pair(StdTags::PY, "py"),
    make_pair(StdTags::CODE, "code"),
    make_pair(StdTags::DIV, "div"),
    make_pair(StdTags::DIV_WARNING, "div_warning"),
    make_pair(StdTags::DIV_TIP, "div_tip"),
    make_pair(StdTags::A_HREF, "a_href"),
    make_pair(StdTags::PKT, "pkt"),
    make_pair(StdTags::BOLD, "b"),
    make_pair(StdTags::QUOTE, "cytat"),
};


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->findWidget->hide();
    ui->findWidget->setCodeEditor(ui->plainTextEdit);
    setUpDocumentStyles();
    ui->plainTextEdit->setFocus();

    connectButtons();
    connect(ui->contextTableWidget, &QTableWidget::cellClicked, this, &MainWindow::onContextTableClicked);
    connect(ui->plainTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateContextRequested);
}

void MainWindow::setUpDocumentStyles()
{
    QFile styleFile(":/styles.css");
    styleFile.open(QIODeviceBase::ReadOnly);
    ui->plainTextEdit->document()->setDefaultStyleSheet(styleFile.readAll());
}

void MainWindow::connectButtons()
{
    connect(ui->button_run, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::RUN], tagsClasses[StdTags::RUN]);
    });
    connect(ui->button_cpp, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::CPP], tagsClasses[StdTags::CPP]);
    });
    connect(ui->button_py, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::PY], tagsClasses[StdTags::PY]);
    });
    connect(ui->button_code, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::CODE], tagsClasses[StdTags::CODE]);
    });
    connect(ui->button_div_tip, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::DIV_TIP], tagsClasses[StdTags::DIV], R"( class="tip")");
    });
    connect(ui->button_div_warning, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::DIV_WARNING], tagsClasses[StdTags::DIV], R"( class="uwaga")");
    });
    connect(ui->button_cytat, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::QUOTE], tagsClasses[StdTags::QUOTE]);
    });
    connect(ui->button_href, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithAHrefTag();
    });
    connect(ui->button_pkt, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::PKT], tagsClasses[StdTags::PKT], " ext");
    });
    connect(ui->button_bold, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::BOLD], tagsClasses[StdTags::BOLD]);
    });
    connect(ui->button_h1, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("h1", "h1");
    });
    connect(ui->button_h2, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("h2", "h2");
    });
    connect(ui->button_h3, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("h3", "h3");
    });
    connect(ui->button_h4, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("h4", "h4");
    });
}

MainWindow::~MainWindow()
{
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
    if (closeApplicationReturningIfClosed())
    {
        QWidget::closeEvent(event);
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::closeApplicationReturningIfClosed()
{
    if (!ui->plainTextEdit->noUnsavedChanges())
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
                ui->plainTextEdit->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
                return false;
            }
        }
        else if (reply == QMessageBox::No)
        {
            return false;
        }
        else if (reply == QMessageBox::Discard)
        {
            ui->plainTextEdit->restoreStateWhichDoesNotRequireSaving(/*discardChanges=*/true);
        }
    }
    close();
    qApp->quit();
    return true;
}

void MainWindow::onUpdateContextRequested()
{
    if (ui->contextTableWidget->isHidden())
    {
        return;
    }

    static QRegularExpression hRegex("\\[(h[1-6])\\](.*?)\\[/\\1\\]");
    static QRegularExpression divRegex("\\[(div)(?:\\s+[^\\]]+)?\\](.*?)\\[/\\1\\]", QRegularExpression::DotMatchesEverythingOption);

    const auto text = ui->plainTextEdit->toPlainText();

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
        insertText2Cell(ui->contextTableWidget, rowNumber, 0, QString::number(lineNumber));
        insertText2Cell(ui->contextTableWidget, rowNumber, 1, tag);
        insertText2Cell(ui->contextTableWidget, rowNumber, 2, text);

        ++rowNumber;
    }
    if (ui->contextTableWidget->columnCount())
    {
        auto initialWidgetWidth = ui->contextTableWidget->width();
        ui->contextTableWidget->resizeColumnToContents(0);
        ui->contextTableWidget->resizeColumnToContents(1);
        auto width4LastColumn = initialWidgetWidth - ui->contextTableWidget->columnWidth(0) - ui->contextTableWidget->columnWidth(2);
        ui->contextTableWidget->setColumnWidth(2, width4LastColumn);
    }
}

void MainWindow::onContextTableClicked(int row, int)
{
    QTableWidgetItem *item = ui->contextTableWidget->item(row, 0);
    if (item) {
        bool ok;
        int lineNumber = item->text().toInt(&ok);
        if (ok) {
            QTextCursor cursor = ui->plainTextEdit->textCursor();
            cursor.movePosition(QTextCursor::Start);
            for (int i = 1; i < lineNumber; ++i) {
                cursor.movePosition(QTextCursor::NextBlock);
            }
            ui->plainTextEdit->setTextCursor(cursor);
            ui->plainTextEdit->ensureCursorVisible();
        }
    }
    ui->plainTextEdit->setFocus();
}

bool MainWindow::onSaveAsPressed()
{
    const auto fileName = chooseFileWithDialog(this, QFileDialog::AcceptSave);
    if (!fileName.isEmpty())
    {
        QFile outputFile(fileName);
        outputFile.open(QIODeviceBase::WriteOnly);
        ui->plainTextEdit->setFileName(fileName);
        return outputFile.write(ui->plainTextEdit->toPlainText().toLatin1()) > -1;
    }
    return false;
}

bool MainWindow::onSavePressed()
{
    auto outputFileName = ui->plainTextEdit->getFileName();
    if (outputFileName.isEmpty())
    {
        return onSaveAsPressed();
    }
    else if (ui->plainTextEdit->noUnsavedChanges())
    {
        return true;
    }
    else
    {
        QFile outputFile(outputFileName);
        outputFile.open(QIODeviceBase::WriteOnly);
        if (outputFile.write(ui->plainTextEdit->toPlainText().toLatin1()) < 0)
        {
            return false;
        }
        outputFile.close();
        return true;
    }
}

void MainWindow::onOpenPressed()
{
    const auto fileName = chooseFileWithDialog(this, QFileDialog::AcceptOpen).trimmed();
    QFile file(fileName);
    if (file.exists())
    {
        file.open(QFile::ReadOnly);
        const auto textFromFile = file.readAll();
        ui->plainTextEdit->setPlainText(textFromFile);
        ui->plainTextEdit->setFileName(fileName);
    }
}

void MainWindow::onCheckTagsPressed()
{
    const auto text = ui->plainTextEdit->toPlainText().toStdString();
    const auto tagsErrors = PairedTagsChecker::checkTags(text);

    ui->errorsInText->clearErrors();
    for (const auto [lineNumber, errorText] : tagsErrors)
    {
        ui->errorsInText->addError(lineNumber, QString::fromStdString(errorText));
    }
}

void MainWindow::putTextBackToCursorPosition(QTextCursor &cursor, QString divClass,
                                             QString selectedText, QString textEnding, QString modifiedText)
{
    if (divClass.size())
    {
        QString html = "<div class=\"" + divClass + "\">"
                       + modifiedText + "</div>";
        cursor.insertHtml(html);
        qDebug() << "HTML:" << html;
    }
    else
        cursor.insertText(modifiedText);

    if (0 == selectedText.size()) {
        auto currentPosition = cursor.position();
        auto positionMovedBeforeTextEnding = currentPosition - textEnding.size();
        cursor.setPosition(positionMovedBeforeTextEnding);
        ui->plainTextEdit->setTextCursor(cursor);
    }

    ui->plainTextEdit->setFocus();
}

void MainWindow::surroundSelectedTextWithTag(QString divClass, QString textBase, QString extraAttributes, bool closable)
{
    auto cursor = ui->plainTextEdit->textCursor();
    QString selectedText = cursor.selectedText();
    qDebug() << "Selected text size: " << selectedText.size();

    const auto textOpening = "[" + textBase + extraAttributes + "]";
    const auto textEnding = closable ? "[/" + textBase + "]" : "";
    QString modifiedText = textOpening + selectedText + textEnding;

    putTextBackToCursorPosition(cursor, divClass, selectedText, textEnding, modifiedText);
}

void MainWindow::surroundSelectedTextWithAHrefTag()
{
    auto cursor = ui->plainTextEdit->textCursor();
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
    qDebug() << "Selected text size: " << selectedText.size() << ", link:" << link << ", rest of text:" << restOfText;

    QString modifiedText = beginOfTag + link;
    if (restOfText.size())
    {
        modifiedText += QString('"') + attributeName + restOfText;
    }
    modifiedText += endOfTag;
    putTextBackToCursorPosition(cursor, "a", selectedText, "", modifiedText);
}
