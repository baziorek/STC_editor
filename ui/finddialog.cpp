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

    connect(ui->foundTextsTreeWidget, &QTreeWidget::itemClicked,
            this, &FindDialog::onResultItemClicked);
}

void FindDialog::onResultItemClicked(QTreeWidgetItem* item, int column)
{
    int line = item->data(0, Qt::UserRole).toInt();
    int offset = item->data(1, Qt::UserRole).toInt();

    emit jumpToLocationRequested(line, offset);
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
        // TODO: Use results of the function instead of searching twice more:
        /*const auto [occurencesCaseSensitive, occurencesCaseInsensitive] = */
        showOccurences(newText);
        const auto text = codeEditor->toPlainText();
        const auto occurencesCaseSensitive = text.count(newText, Qt::CaseSensitive);
        const auto occurencesCaseInsensitive = text.count(newText, Qt::CaseInsensitive);

        auto occurencesCountAsText = QString("Occurences: %1/%2").arg(occurencesCaseSensitive).arg(occurencesCaseInsensitive);
        ui->occurencesLabel->setText(occurencesCountAsText);
    }
}

std::pair<int, int> FindDialog::showOccurences(const QString &searchText)
{
    ui->foundTextsTreeWidget->clear();
    if (searchText.isEmpty() || !codeEditor)
    {
        return {};
    }

    if (auto* delegate = qobject_cast<HighlightDelegate*>(ui->foundTextsTreeWidget->itemDelegateForColumn(2)))
    {
        delegate->setSearchTerm(searchText);
    }

    QTextDocument* doc = codeEditor->document();
    QTextCursor cursor(doc);
    int matchCaseInsensitiveCount{};
    int matchCaseSensitiveCount{};

    const int columnWidth = ui->foundTextsTreeWidget->columnWidth(2);
    QFontMetrics fm(ui->foundTextsTreeWidget->font());
    const int charWidth = fm.horizontalAdvance('x');

    const int maxContextLength = std::max(10, columnWidth / charWidth);

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor, QTextDocument::FindCaseSensitively);
        if (!cursor.isNull())
        {
            matchCaseInsensitiveCount++;

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
            item->setData(0, Qt::UserRole, lineNumber);
            item->setData(1, Qt::UserRole, offset);
            item->setData(2, Qt::DisplayRole, visibleText);

            ui->foundTextsTreeWidget->addTopLevelItem(item);
        }
    }

    ui->foundTextsTreeWidget->setColumnCount(3);
    ui->foundTextsTreeWidget->setHeaderLabels({ "Line", "Offset", "Context" });

    return {matchCaseSensitiveCount, matchCaseInsensitiveCount};
}
