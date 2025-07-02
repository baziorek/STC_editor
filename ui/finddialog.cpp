#include <QMessageBox>
#include <QTextBlock>
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

        showOccurences(newText);
    }
}

void FindDialog::showOccurences(const QString &searchText)
{
    ui->foundTextsTreeWidget->clear();
    if (searchText.isEmpty() || !codeEditor)
        return;

    QTextDocument* doc = codeEditor->document();
    QTextCursor cursor(doc);
    int matchCount = 0;

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor, QTextDocument::FindCaseSensitively);
        if (!cursor.isNull())
        {
            matchCount++;

            int lineNumber = cursor.blockNumber() + 1;
            QString lineText = cursor.block().text();

            auto *item = new QTreeWidgetItem();
            item->setText(0, QString("Line %1").arg(lineNumber));
            item->setText(1, QString::number(cursor.positionInBlock()));
            item->setText(2, lineText.trimmed());
            item->setData(0, Qt::UserRole, lineNumber); // zapisz numer linii
            ui->foundTextsTreeWidget->addTopLevelItem(item);
        }
    }
    qDebug() << "Matches:" << matchCount;
}
