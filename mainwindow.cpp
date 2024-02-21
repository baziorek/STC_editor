#include <cstdint>
#include <QKeyEvent>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QRegularExpression>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
using namespace std;

namespace
{
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
    setUpDocumentStyles();
    connect(ui->plainTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onUpdateContextRequested);
    connect(ui->contextTableWidget, &QTableWidget::cellClicked, this, &MainWindow::onContextTableClicked);

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

void MainWindow::setUpDocumentStyles()
{
    QFile styleFile(":/styles.css");
    styleFile.open(QIODeviceBase::ReadOnly);
    ui->plainTextEdit->document()->setDefaultStyleSheet(styleFile.readAll());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (Qt::Key_Escape == event->key())
    {
        close();
        qApp->quit();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

// TODO: Make finding lines more optimal
void MainWindow::onUpdateContextRequested()
{
    static QRegularExpression hRegex("\\[(h[1-6])\\](.*?)\\[/h[1-6]\\]");
    static QRegularExpression divRegex("\\[div(?:\\s+[^\\]]+)?\\](.*?)\\[/div\\]", QRegularExpression::DotMatchesEverythingOption);
    std::map<int, pair<QString, QString>> textPerLine;

    auto text = ui->plainTextEdit->toPlainText();

    for (QRegularExpressionMatchIterator matches = hRegex.globalMatch(text); matches.hasNext(); )
    {
        QRegularExpressionMatch match = matches.next();
        auto lineNumber = text.left(match.capturedStart(0)).count('\n') + 1;
        textPerLine.insert({lineNumber, make_pair(QString(match.captured(1)), match.captured(2))});
    }

    for (QRegularExpressionMatchIterator matches = divRegex.globalMatch(text); matches.hasNext(); )
    {
        QRegularExpressionMatch match = matches.next();
        auto lineNumber = text.left(match.capturedStart(0)).count('\n') + 1;
        textPerLine.insert({lineNumber, make_pair(QString("div"), match.captured(1))});
    }

    ui->contextTableWidget->setRowCount(textPerLine.size());

    auto insertText2Cell = [this](int row, int column, const QString& text)
    {
        if (auto* cell = ui->contextTableWidget->item(row, column); cell == nullptr)
        {
            cell = new QTableWidgetItem(text);
            cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
            ui->contextTableWidget->setItem(row, column, cell);
        }
        else
        {
            ui->contextTableWidget->item(row, column)->setText(text);
        }
    };
    unsigned rowNumber = 0;
    for (const auto& [lineNumber, tagAndText] : textPerLine)
    {
        const auto& [tag, text] = tagAndText;
        insertText2Cell(rowNumber, 0, QString::number(lineNumber));
        insertText2Cell(rowNumber, 1, tag);
        insertText2Cell(rowNumber, 2, text);

        ++rowNumber;
    }
    if (ui->contextTableWidget->columnCount())
    {
        ui->contextTableWidget->resizeColumnToContents(0);
        ui->contextTableWidget->resizeColumnToContents(1);
        auto width4LastColumn = ui->contextTableWidget->width() - ui->contextTableWidget->columnWidth(0) - ui->contextTableWidget->columnWidth(2);
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

void MainWindow::onSaveAsPressed()
{
    const auto fileName = chooseFileWithDialog(this, QFileDialog::AcceptSave);
    if (!fileName.isEmpty())
    {
        QFile outputFile(fileName);
        outputFile.open(QIODeviceBase::WriteOnly);
        outputFile.write(ui->plainTextEdit->toPlainText().toLatin1());
    }
}

void MainWindow::onOpenPressed()
{ // TODO:
    const auto fileName = chooseFileWithDialog(this, QFileDialog::AcceptOpen).trimmed();
    // if (! FileHandling::fileExists(fileName.toStdString()))
    // {
    //     QMessageBox::warning(this, "Brak pliku!", "Plik o nazwie: '" + fileName + "' nie istnieje!");
    //     return;
    // }

    // if (!fileName.isEmpty())
    // {
    // }
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
