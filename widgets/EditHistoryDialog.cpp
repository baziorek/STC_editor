#include "EditHistoryDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include "EditHistoryFormat.h"
#include "RichTextDelegate.h"
#include "widgets/DiffViewerWidget.h"

EditHistoryModel::EditHistoryModel(EditHistory* history, QObject* parent)
    : QAbstractTableModel(parent), history(history)
{
}

int EditHistoryModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid() || !history) ? 0 : history->stepCount() + 1;
}

int EditHistoryModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

void EditHistoryModel::refresh()
{
    beginResetModel();
    endResetModel();
}

QVariant EditHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    if (role == Qt::DisplayRole)
    {
        switch (section)
        {
        case NumberColumn:      return tr("#");
        case TimeColumn:        return tr("Time");
        case LinesColumn:       return tr("Lines");
        case LineNumbersColumn: return tr("Line numbers");
        case CharsColumn:       return tr("Characters");
        case SavedColumn:       return tr("Saved");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case NumberColumn:
            return tr("Number of the state of the document after the step. 0 is the state it started in.\n"
                      "Ctrl+Z moves the current state (▶) back by one, Ctrl+Shift+Z forward by one.");
        case LinesColumn:
            return tr("How many lines the step changed (modified, added and removed ones)");
        case LineNumbersColumn:
            return tr("Modified lines; +added lines; −removed lines.\n"
                      "Modified and added lines are numbered as after the step, removed ones as before it.");
        case CharsColumn:
            return tr("Characters inserted (+) and removed (−), line breaks included");
        case SavedColumn:
            return tr("When the file was written while the document was in this state");
        }
    }
    return {};
}

QVariant EditHistoryModel::data(const QModelIndex& index, int role) const
{
    if (!history || !index.isValid() || index.row() > history->stepCount())
        return {};

    const EditHistory::StepInfo info = history->stepInfo(index.row());
    const int column = index.column();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (column)
        {
        case NumberColumn:
            return info.isCurrent ? QStringLiteral("▶ %1").arg(info.number) : QString::number(info.number);
        case TimeColumn:
            return EditHistoryFormat::time(info.time);
        case LinesColumn:
            return info.number == 0 ? QStringLiteral("–") : QString::number(info.changedLines());
        case LineNumbersColumn:
            return EditHistoryFormat::lineNumbersPlain(info);
        case CharsColumn:
            return info.number == 0 ? QStringLiteral("–") : EditHistoryFormat::charsPlain(info);
        case SavedColumn:
            return info.saves.isEmpty() ? QString() : EditHistoryFormat::time(info.saves.last());
        }
        break;

    case RichTextDelegate::RichTextRole:
        if (!info.applied)
            break; // undone: plain text, greyed like the rest of the row
        if (column == LineNumbersColumn)
            return EditHistoryFormat::lineNumbersHtml(info);
        if (column == CharsColumn && info.number > 0)
            return EditHistoryFormat::charsHtml(info);
        break;

    case Qt::ToolTipRole:
        switch (column)
        {
        case NumberColumn:
            if (info.number == 0)
                return tr("The state the document started in");
            return info.isCurrent ? tr("The document is in this state now")
                                  : (info.applied ? QString() : tr("Undone: Ctrl+Shift+Z brings it back"));
        case TimeColumn:
            if (info.number == 0)
                return tr("The content was loaded at %1").arg(info.time.toString(Qt::ISODate));
            return tr("Started: %1\nLast edit: %2").arg(info.time.toString(Qt::ISODate), info.lastEditTime.toString(Qt::ISODate));
        case LinesColumn:
            return info.number == 0 ? QString()
                                    : tr("%1 modified, %2 added, %3 removed").arg(info.modifiedLines).arg(info.addedLines).arg(info.removedLines);
        case LineNumbersColumn:
            return info.number == 0 ? QString()
                                    : tr("Modified lines; +added lines; −removed lines.\n"
                                         "Modified and added lines are numbered as after the step, removed ones as before it.\n"
                                         "Double click to go to the line.");
        case CharsColumn:
            return info.charsApproximate ? tr("The texts were too long to compare character by character: the numbers are approximate") : QString();
        case SavedColumn:
        {
            QStringList times;
            for (const QDateTime& saved : info.saves)
                times << saved.toString(Qt::ISODate);
            return times.join(QLatin1Char('\n'));
        }
        }
        break;

    case Qt::ForegroundRole:
        if (!info.applied)
            return QBrush(QApplication::palette().color(QPalette::Disabled, QPalette::Text));
        break;

    case Qt::FontRole:
    {
        QFont font = QApplication::font();
        font.setBold(info.isCurrent);
        font.setItalic(!info.applied || (info.number > 0 && info.changedLines() == 0));
        return font;
    }

    case Qt::BackgroundRole:
        if (info.isCurrent)
        {
            QColor color = QApplication::palette().color(QPalette::Highlight);
            color.setAlpha(55);
            return QBrush(color);
        }
        break;

    case Qt::TextAlignmentRole:
        if (column == NumberColumn || column == LinesColumn || column == TimeColumn || column == SavedColumn)
            return int(Qt::AlignCenter);
        if (column == CharsColumn)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return {};
}


EditHistoryDialog::EditHistoryDialog(EditHistory* history, QWidget* parent)
    : QDialog(parent, Qt::Window), history(history), model(new EditHistoryModel(history, this))
{
    setWindowTitle(tr("Edit history"));
    resize(1020, 680);

    summaryLabel = new QLabel(this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::RichText);

    table = new QTableView(this);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(QFontMetrics(table->font()).height() + 10);
    for (const int column : { EditHistoryModel::LineNumbersColumn, EditHistoryModel::CharsColumn })
    {
        auto* delegate = new RichTextDelegate(false, table);
        delegate->setPlainTextWhenSelected(true);
        table->setItemDelegateForColumn(column, delegate);
    }

    QHeaderView* header = table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(EditHistoryModel::NumberColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(EditHistoryModel::TimeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(EditHistoryModel::LinesColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(EditHistoryModel::LineNumbersColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(EditHistoryModel::CharsColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(EditHistoryModel::SavedColumn, QHeaderView::ResizeToContents);

    diffTitle = new QLabel(this);
    diffTitle->setWordWrap(true);
    diffViewer = new DiffViewerWidget(this);
    diffViewer->setRowActions(DiffViewerWidget::RowActions::None);

    auto* diffPanel = new QWidget(this);
    auto* diffLayout = new QVBoxLayout(diffPanel);
    diffLayout->setContentsMargins(0, 0, 0, 0);
    diffLayout->addWidget(diffTitle);
    diffLayout->addWidget(diffViewer, 1);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(table);
    splitter->addWidget(diffPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summaryLabel);
    layout->addWidget(splitter, 1);
    layout->addWidget(buttons);

    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]() { showSelectedStep(); });
    connect(table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!this->history || index.row() < 1)
            return;
        const int line = this->history->stepLineInEditor(index.row());
        if (line >= 0)
            emit jumpToLineRequested(line);
    });

    // The document keeps changing while this window is open: refresh, but not at every keystroke
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    connect(&refreshTimer, &QTimer::timeout, this, &EditHistoryDialog::refresh);
    connect(history, &EditHistory::changed, &refreshTimer, qOverload<>(&QTimer::start));

    lastCurrent = history->currentIndex();
    updateSummary();
    selectStep(lastCurrent);
}

int EditHistoryDialog::selectedStep() const
{
    const QModelIndex index = table->currentIndex();
    return index.isValid() ? index.row() : -1;
}

void EditHistoryDialog::selectStep(int number)
{
    const QModelIndex index = model->index(number, 0);
    table->setCurrentIndex(index);
    table->selectRow(number);
    table->scrollTo(index, QAbstractItemView::PositionAtCenter);
    showSelectedStep();
}

void EditHistoryDialog::refresh()
{
    if (!history)
        return;

    const int selected = selectedStep();
    const bool following = (selected < 0 || selected == lastCurrent); // not moved away from the current state: stay with it
    lastCurrent = history->currentIndex();

    model->refresh();
    updateSummary();

    const int row = following ? lastCurrent : std::min(selected, history->stepCount());
    selectStep(row);
}

void EditHistoryDialog::updateSummary()
{
    if (!history)
        return;

    const int total = history->stepCount();
    const int current = history->currentIndex();
    QString text = tr("Current state: <b>%1</b> of %2").arg(current).arg(total);
    if (current < total)
        text += tr(" (%1 undone - Ctrl+Shift+Z brings them back)").arg(total - current);

    const auto marks = history->saveMarks();
    text += QStringLiteral("  ·  ");
    if (marks.isEmpty())
    {
        text += tr("not saved in this session");
    }
    else
    {
        const auto& last = marks.last();
        if (last.stateDiscarded)
            text += tr("last saved at %1 (that state is gone: edits were made after an undo)").arg(EditHistoryFormat::time(last.time));
        else if (last.state == current)
            text += tr("<b>saved</b> at %1 (the document is in the saved state)").arg(EditHistoryFormat::time(last.time));
        else
            text += tr("last saved at %1, in state %2").arg(EditHistoryFormat::time(last.time)).arg(last.state);
    }

    const int discarded = history->discardedStepCount();
    if (discarded > 0)
        text += QStringLiteral("<br>") + tr("The text of the oldest %1 steps was dropped to save memory; their numbers are kept.").arg(discarded);

    summaryLabel->setText(text);
}

void EditHistoryDialog::showSelectedStep()
{
    const int number = selectedStep();
    if (!history || number < 0)
    {
        diffViewer->setDiffData({});
        diffTitle->clear();
        return;
    }

    if (number == 0)
    {
        diffViewer->setDiffData({});
        diffTitle->setText(tr("The state the document started in (loaded at %1). Select a step to see what it changed.")
                               .arg(EditHistoryFormat::time(history->baselineTime())));
        return;
    }

    const EditHistory::StepInfo info = history->stepInfo(number);
    const auto diff = history->stepDiff(number);
    diffViewer->setDiffData(diff);

    QString title = tr("Step %1, %2").arg(number).arg(EditHistoryFormat::time(info.time));
    if (!info.applied)
        title += tr(" (undone)");
    title += QStringLiteral(": ");
    if (info.detailsDiscarded)
        title += tr("the text of this step was dropped to save memory.");
    else if (info.changedLines() == 0)
        title += tr("no text changed (only formatting).");
    else
        title += tr("%1 modified, %2 added, %3 removed lines; %4 characters")
                     .arg(info.modifiedLines).arg(info.addedLines).arg(info.removedLines).arg(EditHistoryFormat::charsPlain(info));
    diffTitle->setText(title);
}
