#include "errorlist.h"
#include "ui_errorlist.h"


ErrorList::ErrorList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ErrorList)
{
    ui->setupUi(this);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

ErrorList::~ErrorList()
{
    delete ui;
}

void ErrorList::addError(int lineNumber, int positionInLine, const QString& errorText)
{   // TODO: Add clicable events to table
    setVisible(true);

    const auto rows = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(rows + 1);
    ui->tableWidget->setItem(rows, 0, new QTableWidgetItem{QString::number(lineNumber)});
    ui->tableWidget->setItem(rows, 1, new QTableWidgetItem{QString::number(positionInLine)});
    ui->tableWidget->setItem(rows, 2, new QTableWidgetItem{errorText});

    ui->tableWidget->setVisible(true);
    ui->label->setHidden(true);
}

void ErrorList::clearErrors()
{
    setVisible(true);

    ui->tableWidget->setRowCount(0);
    ui->label->setText("No tags errors!");
    ui->label->setVisible(true);
    ui->tableWidget->setHidden(true);
}
