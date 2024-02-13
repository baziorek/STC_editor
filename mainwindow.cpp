#include <QKeyEvent>
#include <QDebug>
#include <QRegularExpression>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
using namespace std;

namespace
{
std::pair<QString, QString> extractLink(const QString& text) {
    QRegularExpression regex("(https?://\\S+)");
    QRegularExpressionMatch match = regex.match(text);

    if (match.hasMatch()) {
        QString link = match.captured(1);
        QString restOfText = text.mid(match.capturedEnd());
        return std::make_pair(link, restOfText);
    }

    // If no link is found, return an empty pair
    return std::make_pair("", text);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->button_run, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("run");
    });
    connect(ui->button_cpp, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("cpp", "", true, "blue");
    });
    connect(ui->button_py, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("py", "", true, "brown");
    });
    connect(ui->button_code, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("code", "", true, "yellow", "black");
    });
    connect(ui->button_div_tip, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("div", R"( class="tip")", true, "darkgreen");
    });
    connect(ui->button_div_warning, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("div", R"( class="uwaga")", true, "red");
    });
    connect(ui->button_cytat, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("cytat", "", true, "black", "orange");
    });
    connect(ui->button_href, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithAHrefTag("lightblue");
    });
    connect(ui->button_pkt, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("pkt", " ext");
    });
    connect(ui->button_bold, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag("b", "", true, "");
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
        close();
        qApp->quit();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::putTextBackToCursorPosition(QString tagColor, QTextCursor &cursor,
                                             QString selectedText, QString textEnding,
                                             QString modifiedText, QString backgroundColor)
{
    if (tagColor.size())
    {
        QString html = "<div style='color: " + tagColor + ';';
        if (backgroundColor.size())
        {
            html += "background-color:" + backgroundColor + ';';
        }
        html += "'>" + modifiedText + "</div>";
        cursor.insertHtml(html);
        qDebug() << html;
    } else
        cursor.insertText(modifiedText);

    if (0 == selectedText.size()) {
        auto currentPosition = cursor.position();
        auto positionMovedBeforeTextEnding = currentPosition - textEnding.size();
        cursor.setPosition(positionMovedBeforeTextEnding);
        ui->plainTextEdit->setTextCursor(cursor);
    }

    ui->plainTextEdit->setFocus();
}
void MainWindow::surroundSelectedTextWithTag(QString textBase,
                                             QString extraAttributes,
                                             bool closable, QString tagColor, QString backgroundColor) {
    auto cursor = ui->plainTextEdit->textCursor();
    QString selectedText = cursor.selectedText();
    qDebug() << "Selected text size: " << selectedText.size();

    const auto textOpening = "[" + textBase + extraAttributes + "]";
    const auto textEnding = closable ? "[/" + textBase + "]" : "";
    QString modifiedText = textOpening + selectedText + textEnding;

    putTextBackToCursorPosition(tagColor, cursor, selectedText, textEnding, modifiedText, backgroundColor);
}

void MainWindow::surroundSelectedTextWithAHrefTag(QString tagColor)
{
    auto cursor = ui->plainTextEdit->textCursor();
    QString selectedText = cursor.selectedText();

    constexpr const char* beginOfTag = R"([a href=")";
    constexpr const char* endOfTag = R"("])";
    constexpr const char* attributeName = R"( name=")";

    if (0 == selectedText.size())
    {
        const QString modifiedText = QString(beginOfTag) + '"' + attributeName + endOfTag;
        putTextBackToCursorPosition(tagColor, cursor, selectedText, "", modifiedText, "");
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
    putTextBackToCursorPosition(tagColor, cursor, selectedText, "", modifiedText, "");
}
