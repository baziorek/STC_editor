#include <QLineEdit>
#include <QFocusEvent>
#include "gotolinewidget.h"
#include "ui_gotolinewidget.h"


GoToLineWidget::GoToLineWidget(QWidget *parent)
    : QGroupBox(parent)
    , ui(new Ui::GoToLineWidget)
{
    ui->setupUi(this);

    /// reaction for pressing ENTER on keyboard:
    QLineEdit *editor = ui->lineNumberSpinBox->findChild<QLineEdit *>();
    connect(editor, &QLineEdit::returnPressed, this, [this]() {
        emit onGoToLineRequested(ui->lineNumberSpinBox->value());
    });
}

GoToLineWidget::~GoToLineWidget()
{
    delete ui;
}

void GoToLineWidget::setMaxLine(int maxLineNumber)
{
    ui->lineNumberSpinBox->setMaximum(maxLineNumber);
}

void GoToLineWidget::currentLineChanged(int currentLineNumber)
{
    if (currentLineNumber <= ui->lineNumberSpinBox->maximum())
    {
        ui->lineNumberSpinBox->setValue(currentLineNumber);
    }
}

void GoToLineWidget::onGoToLinePressed()
{
    auto lineNumber2Jump = ui->lineNumberSpinBox->value();
    emit onGoToLineRequested(lineNumber2Jump);
}

void GoToLineWidget::focusInEvent(QFocusEvent *event)
{
    if (event->gotFocus())
    {
        ui->lineNumberSpinBox->setFocus();
        ui->lineNumberSpinBox->selectAll();
    }
    QWidget::focusInEvent(event);
}
