#include <QMessageBox>
#include <QTextBlock>
#include <QLineEdit>
#include <QKeyEvent>
#include "finddialog.h"
#include "ui_finddialog.h"
#include "CodeEditor.h"
#include "highlightdelegate.h"


FindDialog::FindDialog(QWidget *parent)
    : QWidget(parent), ui(new Ui::FindDialog)
{
    ui->setupUi(this);

    auto delegate = new HighlightDelegate(this);
    ui->foundTextsTreeWidget->setItemDelegateForColumn(2, delegate);

    connect(ui->foundTextsTreeWidget, &QTreeWidget::itemClicked, this, &FindDialog::onResultItemClicked);

    installEventFilter2HandleMovingBetweenOccurences();
}
void FindDialog::installEventFilter2HandleMovingBetweenOccurences()
{
    if (ui->textSearchField->isEditable())
    {
        ui->textSearchField->installEventFilter(this);
    }
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

void FindDialog::onNextOccurencyPressed()
{
    // Get the number of items in the results tree
    int total = ui->foundTextsTreeWidget->topLevelItemCount();
    if (total == 0)
        return;

    // Find the currently selected item
    QTreeWidgetItem* current = ui->foundTextsTreeWidget->currentItem();
    int currentIndex = -1;
    for (int i = 0; i < total; ++i)
    {
        if (ui->foundTextsTreeWidget->topLevelItem(i) == current)
        {
            currentIndex = i;
            break;
        }
    }
    // Move to next item (wrap around)
    const int nextIndex = (currentIndex + 1) % total;
    QTreeWidgetItem* nextItem = ui->foundTextsTreeWidget->topLevelItem(nextIndex);
    ui->foundTextsTreeWidget->setCurrentItem(nextItem);
    ui->foundTextsTreeWidget->scrollToItem(nextItem);
    // Simulate click to trigger jump
    onResultItemClicked(nextItem, 0);
}

void FindDialog::onPreviousOccurencyPressed()
{
    // Get the number of items in the results tree
    int total = ui->foundTextsTreeWidget->topLevelItemCount();
    if (total == 0)
        return;

    // Find the currently selected item
    QTreeWidgetItem* current = ui->foundTextsTreeWidget->currentItem();
    int currentIndex = -1;
    for (int i = 0; i < total; ++i)
    {
        if (ui->foundTextsTreeWidget->topLevelItem(i) == current)
        {
            currentIndex = i;
            break;
        }
    }
    // Move to previous item (wrap around)
    const int prevIndex = (currentIndex - 1 + total) % total;
    QTreeWidgetItem* prevItem = ui->foundTextsTreeWidget->topLevelItem(prevIndex);
    ui->foundTextsTreeWidget->setCurrentItem(prevItem);
    ui->foundTextsTreeWidget->scrollToItem(prevItem);

    // Simulate click to trigger jump
    onResultItemClicked(prevItem, 0);
}

bool FindDialog::eventFilter(QObject* obj, QEvent* event)
{
    // Handle key events for the search field's QLineEdit
    if (ui->textSearchField->isEditable())
    {
        QLineEdit* edit = ui->textSearchField->lineEdit();
        if ((obj == ui->textSearchField || obj == edit) && event->type() == QEvent::KeyPress)
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            {
                if (keyEvent->modifiers() & Qt::ShiftModifier)
                {
                    // Shift+Enter: previous occurrence
                    onPreviousOccurencyPressed();
                    return true;
                }
                else
                {
                    // Enter: next occurrence
                    onNextOccurencyPressed();
                    return true;
                }
            }
            else if (keyEvent->key() == Qt::Key_Down)
            {
                // Down arrow: next occurrence, keep focus in field
                onNextOccurencyPressed();
                ui->textSearchField->setFocus();
                return true;
            }
            else if (keyEvent->key() == Qt::Key_Up)
            {
                // Up arrow: previous occurrence, keep focus in field
                onPreviousOccurencyPressed();
                ui->textSearchField->setFocus();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
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

        codeEditor->setSearchHighlights({});
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

    // Highlight all matches in the editor
    updateHighlights();
    return stats;
}


void FindDialog::updateHighlights()
{
    if (!codeEditor)
        return;

    QList<QTextEdit::ExtraSelection> highlights;
    if (ui->textSearchField->currentText().isEmpty())
    {
        codeEditor->setSearchHighlights({});
        return;
    }
    QTextDocument* doc = codeEditor->document();
    QTextCursor cursor(doc);
    QTextDocument::FindFlags flags;
    if (ui->matchCasesCheckBox->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    if (ui->wholeWordsCheckBox->isChecked())
        flags |= QTextDocument::FindWholeWords;
    while (!cursor.isNull() && !cursor.atEnd())
    {
        cursor = doc->find(ui->textSearchField->currentText(), cursor, flags);
        if (!cursor.isNull())
        {
            QTextEdit::ExtraSelection sel;
            sel.cursor = cursor;
            sel.format.setBackground(QColor(255, 255, 0, 80));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            highlights.append(sel);
        }
    }
    codeEditor->setSearchHighlights(highlights);
}

void FindDialog::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (! ui->textSearchField->currentText().isEmpty())
        updateHighlights();
}

void FindDialog::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    if (codeEditor)
        codeEditor->setSearchHighlights({});
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
