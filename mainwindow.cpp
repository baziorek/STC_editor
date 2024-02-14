#include <cstdint>
#include <QKeyEvent>
#include <QDebug>
#include <QFile>
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
        this->surroundSelectedTextWithAHrefTag("lightblue");
    });
    connect(ui->button_pkt, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::PKT], tagsClasses[StdTags::PKT], " ext");
    });
    connect(ui->button_bold, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWithTag(tagsClasses[StdTags::BOLD], tagsClasses[StdTags::BOLD]);
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

void MainWindow::putTextBackToCursorPosition(QString tagColor, QTextCursor &cursor,
                                             QString selectedText, QString textEnding,
                                             QString modifiedText, QString backgroundColor, QString divClass)
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

    putTextBackToCursorPosition("", cursor, selectedText, textEnding, modifiedText, "", divClass);
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
        putTextBackToCursorPosition(tagColor, cursor, selectedText, "", modifiedText, "", "a_href");
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
    putTextBackToCursorPosition(tagColor, cursor, selectedText, "", modifiedText, "", "a_href");
}
