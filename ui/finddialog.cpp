#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFlags>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QtGlobal>
#include <algorithm>
#include "finddialog.h"
#include "ui_finddialog.h"
#include "CodeEditor.h"
#include "highlightdelegate.h"


namespace
{
// The data kept by the items of the list of results
constexpr int LineNumberRole = Qt::UserRole;        // column 0
constexpr int OffsetInLineRole = Qt::UserRole;      // column 1
constexpr int DocumentStartRole = Qt::UserRole + 2; // column 0
constexpr int LengthRole = Qt::UserRole + 3;        // column 0
constexpr int FoundTextRole = Qt::UserRole + 4;     // column 0

// A refresh after an edit waits until the editing pauses. After a slow search it waits longer, so a refresh
// of a huge document never blocks the typing again and again.
constexpr int MinRefreshDelayMs = 150;
constexpr int MaxRefreshDelayMs = 3000;
constexpr int SlowSearchFactor = 4;

/// One row of the list of results
struct ResultRow
{
    int lineNumber = 0;     ///< counted from 1
    int offsetInLine = 0;   ///< where a click on the row goes
    int documentStart = 0;  ///< the same place counted from the beginning of the document
    int length = 0;
    QString foundText;      ///< as it stands in the document
    QString context;        ///< the part of the line which the list shows
};

QTreeWidgetItem* makeResultItem(const ResultRow& row)
{
    auto *item = new QTreeWidgetItem();
    item->setText(0, QCoreApplication::translate("FindDialog", "Line %1").arg(row.lineNumber));
    item->setText(1, QString::number(row.offsetInLine));
    item->setData(0, LineNumberRole, row.lineNumber);
    item->setData(1, OffsetInLineRole, row.offsetInLine);
    item->setData(0, DocumentStartRole, row.documentStart);
    item->setData(0, LengthRole, row.length);
    item->setData(0, FoundTextRole, row.foundText);
    item->setData(2, Qt::DisplayRole, row.context);
    return item;
}

/// In the replace mode every row has a check box: the ticked rows are replaced
void makeCheckable(QTreeWidgetItem& item, bool checked)
{
    item.setFlags(item.flags() | Qt::ItemIsUserCheckable);
    item.setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
}

/// The part of the line which the list shows: the place [start, end) with as much text around it as fits
QString contextAround(const QString& line, int start, int end, int maxLength)
{
    const int contextHalf = std::max(0, (maxLength - (end - start)) / 2);
    const qsizetype startContext = std::max<qsizetype>(0, start - contextHalf);
    const qsizetype endContext = std::min<qsizetype>(line.length(), end + contextHalf);

    QString visibleText = line.mid(startContext, endContext - startContext);
    if (startContext > 0)
        visibleText.prepend("…");
    if (endContext < line.length())
        visibleText.append("…");
    return visibleText;
}

/// Where the context of a line with several words ends: after the last word, but not further than the list is wide
int contextEndOfWords(const QList<LineSearch::Occurrence>& places, int maxLength)
{
    int end = 0;
    for (const LineSearch::Occurrence& place : places)
        end = std::max(end, place.end());
    return std::min(end, places.first().start + maxLength);
}

QTextEdit::ExtraSelection makeHighlight(QTextDocument* document, int start, int length)
{
    QTextEdit::ExtraSelection selection;
    selection.cursor = QTextCursor(document);
    selection.cursor.setPosition(start);
    selection.cursor.setPosition(start + length, QTextCursor::KeepAnchor);
    selection.format.setBackground(QColor(255, 255, 0, 80));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    return selection;
}

/// A cursor which follows the edits of the document. The position may come from before the last edit, so it is limited.
QTextCursor cursorAt(QTextDocument* document, int position)
{
    QTextCursor cursor(document);
    cursor.setPosition(std::clamp(position, 0, document->characterCount() - 1));
    return cursor;
}

void countPlace(FindDialog::MatchStats& stats, const LineSearch::Occurrence& place)
{
    ++stats.insensitive;
    if (place.wholeWord)
        ++stats.insensitiveWhole;
    if (place.sameCase)
    {
        ++stats.sensitive;
        if (place.wholeWord)
            ++stats.sensitiveWhole;
    }
}
} // namespace


/// Everything one search produces. The list and the highlights are filled in when the search is done.
struct FindDialog::SearchOutcome
{
    MatchStats stats;
    QList<QTreeWidgetItem*> items;
    QList<QTextEdit::ExtraSelection> highlights;
};


FindDialog::FindDialog(QWidget *parent)
    : QWidget(parent), ui(new Ui::FindDialog)
{
    ui->setupUi(this);

    resultsDelegate = new HighlightDelegate(this);
    ui->foundTextsTreeWidget->setItemDelegateForColumn(2, resultsDelegate);

    connect(ui->foundTextsTreeWidget, &QTreeWidget::itemClicked, this, &FindDialog::onResultItemClicked);
    connect(ui->foundTextsTreeWidget, &QTreeWidget::itemChanged, this, &FindDialog::onResultItemChanged);
    connect(ui->foundTextsTreeWidget, &QTreeWidget::currentItemChanged, this, &FindDialog::rememberCurrentMatch);
    connect(ui->replaceTextField, &QComboBox::currentTextChanged, this, &FindDialog::onReplacementCriteriaChanged);
    connect(ui->skipAlreadyReplacedCheckBox, &QCheckBox::toggled, this, &FindDialog::onReplacementCriteriaChanged);
    connect(ui->selectAllMatchesButton, &QPushButton::clicked, this, &FindDialog::onSelectAllMatchesPressed);
    connect(ui->deselectAllMatchesButton, &QPushButton::clicked, this, &FindDialog::onDeselectAllMatchesPressed);
    connect(ui->replaceSelectedMatchesButton, &QPushButton::clicked, this, &FindDialog::onReplaceSelectedMatchesPressed);

    refreshTimer.setSingleShot(true);
    connect(&refreshTimer, &QTimer::timeout, this, &FindDialog::refreshResults);

    installEventFilter2HandleMovingBetweenOccurences();
    setReplaceMode(false);
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
    int line = item->data(0, LineNumberRole).toInt();
    int offset = item->data(1, OffsetInLineRole).toInt();

    emit jumpToLocationRequested(line, offset);
}

void FindDialog::setReplaceMode(bool enabled)
{
    replaceMode = enabled;
    ui->replaceOptionsWidget->setVisible(enabled);
    ui->anyOrderCheckBox->setEnabled(!enabled); // words scattered over a line have no single place to replace

    const QString searchText = ui->textSearchField->currentText();
    if (!searchText.isEmpty())
        currentTextChanged(searchText);
}

bool FindDialog::isReplaceMode() const
{
    return replaceMode;
}

void FindDialog::odCheckboxMatchCasesChanged(bool checked)
{
    auto currentText = ui->textSearchField->currentText();
    if (! currentText.isEmpty())
    {
        emit currentTextChanged(currentText);
    }
}

void FindDialog::onReplacementCriteriaChanged()
{
    if (!isReplaceMode())
        return;

    // The replacement text only changes the candidate list when conditional
    // replacement is enabled. Otherwise retain the user's checked matches.
    if (sender() == ui->replaceTextField && !ui->skipAlreadyReplacedCheckBox->isChecked())
        return;

    const QString searchText = ui->textSearchField->currentText();
    if (!searchText.isEmpty())
        currentTextChanged(searchText);
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
    // A new text or new options mean a new list of candidates, so nothing is remembered from the previous one
    forgetUncheckedMatches();
    currentMatchStart = QTextCursor();

    rebuildResults(newText);
}

void FindDialog::rebuildResults(const QString& newText)
{
    if (!codeEditor)
    {
        QMessageBox::warning(this, "Uninitialised widget!", "It should never happen!");
        return;
    }

    if (newText.isEmpty())
    {
        ui->occurencesLabel->setText(tr("Occurrences: (empty text)"));

        ui->foundTextsTreeWidget->clear();
        updateReplacementControls();

        codeEditor->setSearchHighlights({});
        return;
    }

    QElapsedTimer stopwatch;
    stopwatch.start();
    const MatchStats stats = showOccurences(newText);
    lastSearchMs = static_cast<int>(stopwatch.elapsed());

    QString occurencesCountAsText = occurrencesLabelText(stats);
    if (isReplaceMode())
    {
        occurencesCountAsText += tr(" | Replace candidates: %1")
            .arg(ui->foundTextsTreeWidget->topLevelItemCount());
    }
    ui->occurencesLabel->setText(occurencesCountAsText);

    rememberUncheckedMatches();
    updateReplacementControls();
}

QString FindDialog::occurrencesLabelText(const MatchStats& stats) const
{
    if (stats.anyOrder)
        return tr("Lines: %1").arg(stats.lines);

    if (stats.isZero())
        return tr("Occurrences: 0");

    return tr("Occurrences: %1 (%2)/%3 (%4)")
        .arg(stats.sensitive).arg(stats.sensitiveWhole)
        .arg(stats.insensitive).arg(stats.insensitiveWhole);
}

LineSearch::Criteria FindDialog::currentCriteria() const
{
    return { ui->matchCasesCheckBox->isChecked(), ui->wholeWordsCheckBox->isChecked() };
}

bool FindDialog::isAnyOrderSearch() const
{
    // Replacing needs one place to replace, so then the text is always searched as it is
    return ui->anyOrderCheckBox->isChecked() && !isReplaceMode();
}

int FindDialog::maxContextLength() const
{
    const int columnWidth = ui->foundTextsTreeWidget->columnWidth(2);
    QFontMetrics fm(ui->foundTextsTreeWidget->font());
    const int charWidth = fm.horizontalAdvance('x');
    return std::max(10, columnWidth / charWidth);
}

FindDialog::MatchStats FindDialog::showOccurences(const QString &searchText)
{
    ui->foundTextsTreeWidget->clear();
    if (searchText.isEmpty() || !codeEditor)
    {
        return {};
    }

    // With "Any order" the text is split into words: several of them are looked for in lines, one is looked for as it is
    const QStringList words = isAnyOrderSearch() ? LineSearch::splitIntoWords(searchText) : QStringList{};
    resultsDelegate->setSearchTerms(words.isEmpty() ? QStringList{ searchText } : words);

    SearchOutcome outcome;
    outcome.stats.anyOrder = words.size() > 1;
    if (outcome.stats.anyOrder)
        collectLinesWithAllWords(words, outcome);
    else
        collectExactOccurrences(words.isEmpty() ? searchText : words.first(), outcome);

    ui->foundTextsTreeWidget->addTopLevelItems(outcome.items);
    ui->foundTextsTreeWidget->setColumnCount(3);
    ui->foundTextsTreeWidget->setHeaderLabels({ tr("Line"), tr("Offset"), tr("Context") });

    // Highlight all matches in the editor.
    codeEditor->setSearchHighlights(outcome.highlights);
    return outcome.stats;
}

void FindDialog::collectExactOccurrences(const QString& searchText, SearchOutcome& outcome)
{
    QTextDocument* doc = codeEditor->document();
    const LineSearch::Criteria criteria = currentCriteria();
    const int contextLength = maxContextLength();
    const QSet<int> unchecked = uncheckedMatchPositions();

    const bool skipAlreadyReplaced = isReplaceMode() && ui->skipAlreadyReplacedCheckBox->isChecked();
    const QString documentText = skipAlreadyReplaced ? doc->toPlainText() : QString();

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
    {
        const QString line = block.text();
        for (const LineSearch::Occurrence& place : LineSearch::findOccurrences(line, searchText))
        {
            countPlace(outcome.stats, place);

            // Show only if it fits the options of the user
            if (!place.fits(criteria))
                continue;

            const int startInDoc = block.position() + place.start;
            outcome.highlights.append(makeHighlight(doc, startInDoc, place.length));

            if (skipAlreadyReplaced && isMatchAlreadyReplacement(documentText, startInDoc, place.length))
                continue;

            ResultRow row;
            row.lineNumber = block.blockNumber() + 1;
            row.offsetInLine = place.start;
            row.documentStart = startInDoc;
            row.length = place.length;
            row.foundText = line.mid(place.start, place.length);
            row.context = contextAround(line, place.start, place.end(), contextLength);

            QTreeWidgetItem* item = makeResultItem(row);
            if (isReplaceMode())
                makeCheckable(*item, !unchecked.contains(startInDoc));
            outcome.items.append(item);
        }
    }
}

void FindDialog::collectLinesWithAllWords(const QStringList& words, SearchOutcome& outcome)
{
    QTextDocument* doc = codeEditor->document();
    const LineSearch::Criteria criteria = currentCriteria();
    const int contextLength = maxContextLength();

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next())
    {
        const QString line = block.text();
        const QList<LineSearch::Occurrence> places = LineSearch::findAllWords(line, words, criteria);
        if (places.isEmpty())
            continue;

        // One row for the whole line, which leads to the first of its words
        const LineSearch::Occurrence& first = places.first();
        const int startInDoc = block.position() + first.start;
        ++outcome.stats.lines;
        outcome.highlights.append(makeHighlight(doc, startInDoc, first.length));

        ResultRow row;
        row.lineNumber = block.blockNumber() + 1;
        row.offsetInLine = first.start;
        row.documentStart = startInDoc;
        row.length = first.length;
        row.foundText = line.mid(first.start, first.length);
        row.context = contextAround(line, first.start, contextEndOfWords(places, contextLength), contextLength);
        outcome.items.append(makeResultItem(row));
    }
}

bool FindDialog::isMatchAlreadyReplacement(const QString& documentText, int matchStart, int matchLength) const
{
    const QString replacementText = ui->replaceTextField->currentText();
    if (replacementText.isEmpty() || replacementText.length() < matchLength)
        return false;

    // A replacement already in the document has to contain the entire match.
    // For example, the match "cout" at position 4 is contained by
    // "std::cout" starting at position 0, so it must be skipped.
    const qsizetype firstReplacementStart = std::max<qsizetype>(
        0, static_cast<qsizetype>(matchStart) + matchLength - replacementText.length());
    const qsizetype lastReplacementStart = std::min<qsizetype>(
        matchStart, documentText.length() - replacementText.length());
    if (firstReplacementStart > lastReplacementStart)
        return false;

    const Qt::CaseSensitivity caseSensitivity = ui->matchCasesCheckBox->isChecked()
        ? Qt::CaseSensitive
        : Qt::CaseInsensitive;
    for (qsizetype replacementStart = firstReplacementStart;
         replacementStart <= lastReplacementStart; ++replacementStart)
    {
        if (documentText.mid(replacementStart, replacementText.length())
                .compare(replacementText, caseSensitivity) == 0)
            return true;
    }
    return false;
}

void FindDialog::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // The document could change while the widget was hidden, so the places are searched again
    startFollowingDocument();
    refreshResults();
}

void FindDialog::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);

    // Nobody sees the places now, so they are not kept up to date
    stopFollowingDocument();
    if (codeEditor)
        codeEditor->setSearchHighlights({});
}

void FindDialog::startFollowingDocument()
{
    if (!codeEditor)
        return;

    stopFollowingDocument();
    documentChangesConnection = connect(codeEditor->document(), &QTextDocument::contentsChanged,
                                        this, &FindDialog::scheduleRefresh);
}

void FindDialog::stopFollowingDocument()
{
    disconnect(documentChangesConnection);
    refreshTimer.stop();
}

void FindDialog::scheduleRefresh()
{
    // Every edit restarts the waiting, so a series of edits makes one refresh
    if (!ui->textSearchField->currentText().isEmpty())
        refreshTimer.start(refreshDelayMs());
}

int FindDialog::refreshDelayMs() const
{
    return std::clamp(lastSearchMs * SlowSearchFactor, MinRefreshDelayMs, MaxRefreshDelayMs);
}

void FindDialog::refreshResults()
{
    refreshTimer.stop();

    const QString searchText = ui->textSearchField->currentText();
    if (!codeEditor || searchText.isEmpty())
        return;

    // Same search as when the user types, but what the user did with the results is kept
    rebuildResults(searchText);
    restoreCurrentResult();
}

void FindDialog::rememberCurrentMatch(QTreeWidgetItem* current)
{
    // Clearing the list "changes" the current item to nothing, but then the old place is the one to keep
    if (!current || !codeEditor)
        return;

    currentMatchStart = cursorAt(codeEditor->document(), current->data(0, DocumentStartRole).toInt());
}

void FindDialog::restoreCurrentResult()
{
    if (currentMatchStart.isNull())
        return;

    // The match which the user went to last. When it is gone, the closest one before it, so "next" goes on from there.
    const int position = currentMatchStart.position();
    QTreeWidgetItem* closest = nullptr;
    for (int index = 0; index < ui->foundTextsTreeWidget->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* item = ui->foundTextsTreeWidget->topLevelItem(index);
        if (item->data(0, DocumentStartRole).toInt() > position)
            break;
        closest = item;
    }

    if (closest)
        ui->foundTextsTreeWidget->setCurrentItem(closest);
}

void FindDialog::rememberUncheckedMatches()
{
    uncheckedMatchStarts.clear();
    if (!isReplaceMode() || !codeEditor)
        return;

    for (int index = 0; index < ui->foundTextsTreeWidget->topLevelItemCount(); ++index)
    {
        const QTreeWidgetItem* item = ui->foundTextsTreeWidget->topLevelItem(index);
        if (item->checkState(0) == Qt::Unchecked)
            uncheckedMatchStarts.append(cursorAt(codeEditor->document(), item->data(0, DocumentStartRole).toInt()));
    }
}

void FindDialog::forgetUncheckedMatches()
{
    uncheckedMatchStarts.clear();
}

QSet<int> FindDialog::uncheckedMatchPositions() const
{
    QSet<int> positions;
    for (const QTextCursor& start : uncheckedMatchStarts)
        positions.insert(start.position());
    return positions;
}

void FindDialog::focusInput(bool preferReplacementField)
{
    QComboBox* field = ui->textSearchField;
    if (preferReplacementField && isReplaceMode() && !ui->textSearchField->currentText().isEmpty())
        field = ui->replaceTextField;

    field->setFocus();

    if (field->isEditable())
    {
        if (QLineEdit* edit = field->lineEdit())
        {
            edit->selectAll();
        }
    }
}

void FindDialog::onSelectAllMatchesPressed()
{
    setCheckStateOfAllMatches(Qt::Checked);
}

void FindDialog::onDeselectAllMatchesPressed()
{
    setCheckStateOfAllMatches(Qt::Unchecked);
}

void FindDialog::setCheckStateOfAllMatches(Qt::CheckState state)
{
    {
        // One update at the end instead of one for each of the items
        const QSignalBlocker blocker(ui->foundTextsTreeWidget);
        for (int index = 0; index < ui->foundTextsTreeWidget->topLevelItemCount(); ++index)
            ui->foundTextsTreeWidget->topLevelItem(index)->setCheckState(0, state);
    }

    rememberUncheckedMatches();
    updateReplacementControls();
}

void FindDialog::onResultItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 0 && isReplaceMode())
    {
        rememberUncheckedMatches();
        updateReplacementControls();
    }
}

void FindDialog::updateReplacementControls()
{
    if (!isReplaceMode())
        return;

    int selectedMatches = 0;
    for (int index = 0; index < ui->foundTextsTreeWidget->topLevelItemCount(); ++index)
    {
        if (ui->foundTextsTreeWidget->topLevelItem(index)->checkState(0) == Qt::Checked)
            ++selectedMatches;
    }

    ui->replaceSelectedMatchesButton->setEnabled(selectedMatches > 0 && !ui->textSearchField->currentText().isEmpty());
    ui->replaceSelectedMatchesButton->setText(tr("Replace selected (%1)").arg(selectedMatches));
}

void FindDialog::onReplaceSelectedMatchesPressed()
{
    if (!codeEditor)
        return;

    struct SelectedMatch
    {
        int start;
        int length;
        QString expectedText;
    };

    QList<SelectedMatch> selectedMatches;
    for (int index = 0; index < ui->foundTextsTreeWidget->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem* item = ui->foundTextsTreeWidget->topLevelItem(index);
        if (item->checkState(0) == Qt::Checked)
        {
            selectedMatches.append({
                item->data(0, DocumentStartRole).toInt(),
                item->data(0, LengthRole).toInt(),
                item->data(0, FoundTextRole).toString()
            });
        }
    }

    std::sort(selectedMatches.begin(), selectedMatches.end(), [](const SelectedMatch& left, const SelectedMatch& right) {
        return left.start > right.start;
    });

    QTextDocument* document = codeEditor->document();
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    int replacements = 0;
    for (const SelectedMatch& match : selectedMatches)
    {
        if (match.start < 0 || match.length < 0 ||
            match.start + match.length > document->characterCount() - 1)
            continue;

        cursor.setPosition(match.start);
        cursor.setPosition(match.start + match.length, QTextCursor::KeepAnchor);

        // Results can become stale when the document was edited after searching.
        // Do not overwrite text that is no longer the result the user selected.
        const Qt::CaseSensitivity caseSensitivity = ui->matchCasesCheckBox->isChecked()
            ? Qt::CaseSensitive
            : Qt::CaseInsensitive;
        if (cursor.selectedText().compare(match.expectedText, caseSensitivity) != 0)
            continue;

        cursor.insertText(ui->replaceTextField->currentText());
        ++replacements;
    }
    cursor.endEditBlock();

    if (replacements > 0)
    {
        const QString replacementText = ui->replaceTextField->currentText();
        if (!replacementText.isEmpty() && ui->replaceTextField->findText(replacementText) < 0)
            ui->replaceTextField->addItem(replacementText);
    }

    // The matches which the user did not tick are still in the document, so they stay unticked
    refreshResults();
}
