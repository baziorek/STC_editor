#include <iostream>
#include <QKeyEvent>
#include <QDebug>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->button_run, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("run");
    });
    connect(ui->button_cpp, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("cpp", "", true, "blue");
    });
    connect(ui->button_py, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("py", "", true, "brown");
    });
    connect(ui->button_div_tip, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("div", R"( class="tip")", true, "darkgreen");
    });
    connect(ui->button_div_warning, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("div", R"( class="uwaga")", true, "red");
    });
    connect(ui->button_href, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("a", R"( href="" name="")", false, "lightblue");
    });
    connect(ui->button_pkt, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("pkt", " ext");
    });
    connect(ui->button_bold, &QPushButton::clicked, [this](bool) {
        this->surroundSelectedTextWith("b", "", true, "");
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

void MainWindow::surroundSelectedTextWith(QString textBase, QString extraAttributes, bool closable, QString tagColor)
{
    const auto textOpening = "[" + textBase + extraAttributes + "]";
    const auto textEnding = closable ? "[/" + textBase + "]" : "";
    auto cursor = ui->plainTextEdit->textCursor();
    // Step 2: Get the currently selected text
    QString selectedText = cursor.selectedText();

    cout << selectedText.size() << endl;

    // Step 3: Modify the selected text by adding '%' as surrounding characters
    QString modifiedText = textOpening + selectedText + textEnding;

    // Step 4: Set the modified text back to the cursor's position
    if (tagColor.size())
    {
        QString html = "<div style='color: " + tagColor + ";'>" + modifiedText +"</div>";
        cursor.insertHtml(html);
    }
    else
        cursor.insertText(modifiedText);

    if (0 == selectedText.size())
    {
        auto currentPosition = cursor.position();
        auto positionMovedBeforeTextEnding = currentPosition - textEnding.size();
        cursor.setPosition(positionMovedBeforeTextEnding);
        ui->plainTextEdit->setTextCursor(cursor);
    }

    ui->plainTextEdit->setFocus();
}
