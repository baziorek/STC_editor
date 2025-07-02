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

    constexpr int maxContextLength = 80; // ile znaków ma się mieścić w kolumnie

    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(searchText, cursor, QTextDocument::FindCaseSensitively);
        if (!cursor.isNull())
        {
            matchCount++;

            int lineNumber = cursor.blockNumber() + 1;
            int offset = cursor.positionInBlock();
            QString lineText = cursor.block().text();

            // Przygotuj kontekst z pogrubionym tekstem i przycięciem
            QString visibleText;
            int contextHalf = (maxContextLength - searchText.length()) / 2;

            int startContext = std::max(0, offset - contextHalf);
            int endContext = std::min(lineText.length(), offset + searchText.length() + contextHalf);

            visibleText = lineText.mid(startContext, endContext - startContext);

            // Dodaj wielokropek na początku/końcu jeśli trzeba
            if (startContext > 0)
                visibleText.prepend("…");
            if (endContext < lineText.length())
                visibleText.append("…");

            // Pogrubienie znalezionego tekstu
            QString highlightedText = visibleText;
            int relIndex = highlightedText.indexOf(searchText, Qt::CaseSensitive);
            if (relIndex != -1)
            {
                highlightedText.insert(relIndex + searchText.length(), "</b>");
                highlightedText.insert(relIndex, "<b>");
            }

            // auto *item = new QTreeWidgetItem();
            // item->setText(0, QString("Line %1").arg(lineNumber));
            // item->setText(1, QString::number(offset));

            // // ✨ Ustawiamy jako rich text
            // item->setData(2, Qt::DisplayRole, highlightedText);
            // ui->foundTextsTreeWidget->addTopLevelItem(item);
            auto *item = new QTreeWidgetItem();
            item->setText(0, QString("Line %1").arg(lineNumber));
            item->setText(1, QString::number(offset));
            ui->foundTextsTreeWidget->addTopLevelItem(item);

            // 🔁 Zamiast setText(2, ...) — tworzymy QLabel
            auto label = new QLabel;
            label->setTextFormat(Qt::RichText);
            label->setTextInteractionFlags(Qt::NoTextInteraction);
            label->setText(highlightedText);
            label->setStyleSheet("QLabel { padding: 0px; }");
            ui->foundTextsTreeWidget->setItemWidget(item, 2, label);
        }
    }

    // 🔧 Ustaw kolumny jeśli nie masz wcześniej
    ui->foundTextsTreeWidget->setColumnCount(3);
    ui->foundTextsTreeWidget->setHeaderLabels({ "Line", "Offset", "Context" });

    qDebug() << searchText << ", Matches:" << matchCount;
}
