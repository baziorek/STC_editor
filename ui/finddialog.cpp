#include <QMessageBox>
#include <QTextBlock>
#include <QLineEdit>
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

void FindDialog::odCheckboxMatchCasesChanged(bool checked)
{
    auto currentText = ui->textSearchField->currentText();
    if (! currentText.isEmpty())
    {
        emit currentTextChanged(currentText);
    }
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
        const MatchStats stats = showOccurences(newText);
        QString occurencesCountAsText;
        if (stats.isZero())
        {
            occurencesCountAsText = QString("Occurences: 0");
        }
        else
        {
            occurencesCountAsText = QString("Occurences: %1 (%2)/%3 (%4)")
            .arg(stats.sensitive).arg(stats.sensitiveWhole)
                .arg(stats.insensitive).arg(stats.insensitiveWhole);
        }

        ui->occurencesLabel->setText(occurencesCountAsText);
    }
}

FindDialog::MatchStats FindDialog::showOccurences(const QString &searchText)
{
    const bool caseSensitiveRequired = ui->matchCasesCheckBox->isChecked();
    const bool wholeWordRequired = ui->wholeWordsCheckBox->isChecked();

    ui->foundTextsTreeWidget->clear();
    if (searchText.isEmpty() || !codeEditor)
    {
        return {};
    }

    if (auto* delegate = qobject_cast<HighlightDelegate*>(ui->foundTextsTreeWidget->itemDelegateForColumn(2)))
        delegate->setSearchTerm(searchText);

    QTextDocument* doc = codeEditor->document();
    QTextCursor cursor(doc);
    MatchStats stats;

    const int columnWidth = ui->foundTextsTreeWidget->columnWidth(2);
    QFontMetrics fm(ui->foundTextsTreeWidget->font());
    const int charWidth = fm.horizontalAdvance('x');
    const int maxContextLength = std::max(10, columnWidth / charWidth);

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor); // default: case insensitive
        if (cursor.isNull())
        {
            break;
        }

        QString blockText = cursor.block().text();
        int startInDoc = cursor.selectionStart();
        int endInDoc = cursor.selectionEnd();
        int blockPosition = cursor.block().position();
        int startInBlock = startInDoc - blockPosition;
        int endInBlock = endInDoc - blockPosition;

        QString foundText = blockText.mid(startInBlock, endInBlock - startInBlock);
        bool isCaseSensitive = (foundText == searchText);

        bool leftBoundary = startInBlock == 0 || !blockText[startInBlock - 1].isLetterOrNumber();
        bool rightBoundary = endInBlock >= blockText.length() || !blockText[endInBlock].isLetterOrNumber();
        bool isWholeWord = leftBoundary && rightBoundary;

        stats.insensitive++;
        if (isWholeWord)
            stats.insensitiveWhole++;
        if (isCaseSensitive)
        {
            stats.sensitive++;
            if (isWholeWord)
                stats.sensitiveWhole++;
        }

        // Pokaż tylko jeśli pasuje do opcji użytkownika
        if ((caseSensitiveRequired && !isCaseSensitive) ||
            (wholeWordRequired && !isWholeWord))
            continue;

        int lineNumber = cursor.blockNumber() + 1;
        int contextHalf = (maxContextLength - searchText.length()) / 2;
        auto startContext = std::max<qsizetype>(0, startInBlock - contextHalf);
        auto endContext = std::min<qsizetype>(blockText.length(), endInBlock + contextHalf);

        QString visibleText = blockText.mid(startContext, endContext - startContext);
        if (startContext > 0)
            visibleText.prepend("…");
        if (endContext < blockText.length())
            visibleText.append("…");

        auto *item = new QTreeWidgetItem();
        item->setText(0, QString("Line %1").arg(lineNumber));
        item->setText(1, QString::number(startInBlock));
        item->setData(0, Qt::UserRole, lineNumber);
        item->setData(1, Qt::UserRole, startInBlock);
        item->setData(2, Qt::DisplayRole, visibleText);
        ui->foundTextsTreeWidget->addTopLevelItem(item);
    }

    ui->foundTextsTreeWidget->setColumnCount(3);
    ui->foundTextsTreeWidget->setHeaderLabels({ "Line", "Offset", "Context" });

    return stats;
}


void FindDialog::focusInput()
{
    ui->textSearchField->setFocus();

    if (ui->textSearchField->isEditable())
    {
        if (QLineEdit* edit = ui->textSearchField->lineEdit())
        {
            edit->selectAll();
        }
    }
}
