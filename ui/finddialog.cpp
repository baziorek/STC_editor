#include <QMessageBox>
#include <QTextBlock>
#include "finddialog.h"
#include "ui_finddialog.h"
#include "codeeditor.h"
#include "highlightdelegate.h"


FindDialog::FindDialog(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FindDialog)
{
    ui->setupUi(this);

    auto delegate = new HighlightDelegate(this);
    ui->foundTextsTreeWidget->setItemDelegateForColumn(2, delegate);
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
    {
        return;
    }

    if (auto* delegate = qobject_cast<HighlightDelegate*>(ui->foundTextsTreeWidget->itemDelegateForColumn(2)))
    {
        delegate->setSearchTerm(searchText);
    }

    QTextDocument* doc = codeEditor->document();
    QTextCursor cursor(doc);
    int matchCount = 0;

    constexpr int maxContextLength = 80;

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor, QTextDocument::FindCaseSensitively);
        if (!cursor.isNull())
        {
            matchCount++;

            int lineNumber = cursor.blockNumber() + 1;
            int offset = cursor.positionInBlock();
            QString lineText = cursor.block().text();

            int contextHalf = (maxContextLength - searchText.length()) / 2;
            int startContext = std::max(0, offset - contextHalf);
            int endContext = std::min(lineText.length(), offset + searchText.length() + contextHalf);

            QString visibleText = lineText.mid(startContext, endContext - startContext);
            if (startContext > 0)
                visibleText.prepend("…");
            if (endContext < lineText.length())
                visibleText.append("…");

            auto *item = new QTreeWidgetItem();
            item->setText(0, QString("Line %1").arg(lineNumber));
            item->setText(1, QString::number(offset));
            item->setData(2, Qt::DisplayRole, visibleText);

            ui->foundTextsTreeWidget->addTopLevelItem(item);
        }
    }

    ui->foundTextsTreeWidget->setColumnCount(3);
    ui->foundTextsTreeWidget->setHeaderLabels({ "Line", "Offset", "Context" });

    qDebug() << searchText << ", Matches:" << matchCount;
}
