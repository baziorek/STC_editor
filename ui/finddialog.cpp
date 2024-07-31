#include <QMessageBox>
#include "finddialog.h"
#include "ui_finddialog.h"
#include "codeeditor.h"


FindDialog::FindDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindDialog)
{
    ui->setupUi(this);
}

FindDialog::~FindDialog()
{
    delete ui;
}

void FindDialog::currentTextChanged(QString newText)
{
    if (!codeEditor)
    {
        QMessageBox::warning(this, "Uninitialised widget!", "It should never happen!");
        return;
    }

    if (newText.isEmpty())
    {
        ui->occurencesLabel->setText("Occurences: (empty text)");
    }
    else
    {
        const auto text = codeEditor->toPlainText();
        const auto occurencesCaseSensitive = text.count(newText, Qt::CaseSensitive);
        const auto occurencesCaseInsensitive = text.count(newText, Qt::CaseInsensitive);

        auto occurencesCountAsText = QString("Occurences: %1/%2").arg(occurencesCaseSensitive).arg(occurencesCaseInsensitive);
        ui->occurencesLabel->setText(occurencesCountAsText);
    }
}
