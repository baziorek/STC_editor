#include "errorlist.h"
#include "ui_errorlist.h"

ErrorList::ErrorList(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ErrorList)
{
    ui->setupUi(this);
    setHidden(true);
}

ErrorList::~ErrorList()
{
    delete ui;
}

void ErrorList::addError(int errorNumber, QString errorText)
{   // TODO: Add clicable events to table
    setVisible(true);

    const auto rows = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(rows + 1);
    ui->tableWidget->setItem(rows, 0, new QTableWidgetItem{QString::number(errorNumber)});
    ui->tableWidget->setItem(rows, 1, new QTableWidgetItem{errorText});

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
