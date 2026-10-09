#include "EditHistoryDialog.h"

#include <QApplication>
#include <QFontMetrics>
#include <QHeaderView>

#include "EditHistoryFormat.h"
#include "RichTextDelegate.h"
#include "ui_EditHistoryDialog.h"

namespace
{
using Info = EditHistory::StepInfo;

QString displayText(const Info& info, int column)
{
    switch (column)
    {
    case EditHistoryModel::NumberColumn:
        return info.isCurrent ? QStringLiteral("▶ %1").arg(info.number) : QString::number(info.number);
    case EditHistoryModel::TimeColumn:
        return EditHistoryFormat::time(info.time);
    case EditHistoryModel::LinesColumn:
        return info.number == 0 ? QStringLiteral("–") : QString::number(info.changedLines());
    case EditHistoryModel::LineNumbersColumn:
        return EditHistoryFormat::lineNumbersPlain(info);
    case EditHistoryModel::CharsColumn:
        return info.number == 0 ? QStringLiteral("–") : EditHistoryFormat::charsPlain(info);
    case EditHistoryModel::SavedColumn:
        return info.saves.isEmpty() ? QString() : EditHistoryFormat::time(info.saves.last());
    }
    return {};
}

/// The coloured text of a cell; nothing for an undone step, which is plain grey text like the rest of its row
QVariant richText(const Info& info, int column)
{
    if (!info.applied)
        return {};
    if (column == EditHistoryModel::LineNumbersColumn)
        return EditHistoryFormat::lineNumbersHtml(info);
    if (column == EditHistoryModel::CharsColumn && info.number > 0)
        return EditHistoryFormat::charsHtml(info);
    return {};
}

QString numberToolTip(const Info& info)
{
    if (info.number == 0)
        return EditHistoryModel::tr("The state the document started in");
    if (info.isCurrent)
        return EditHistoryModel::tr("The document is in this state now");
    return info.applied ? QString() : EditHistoryModel::tr("Undone: Ctrl+Shift+Z brings it back");
}

QString timeToolTip(const Info& info)
{
    if (info.number == 0)
        return EditHistoryModel::tr("The content was loaded at %1").arg(info.time.toString(Qt::ISODate));
    return EditHistoryModel::tr("Started: %1\nLast edit: %2")
        .arg(info.time.toString(Qt::ISODate), info.lastEditTime.toString(Qt::ISODate));
}

QString savedToolTip(const Info& info)
{
    QStringList times;
    for (const QDateTime& saved : info.saves)
        times << saved.toString(Qt::ISODate);
    return times.join(QLatin1Char('\n'));
}

QString toolTipText(const Info& info, int column)
{
    switch (column)
    {
    case EditHistoryModel::NumberColumn:
        return numberToolTip(info);
    case EditHistoryModel::TimeColumn:
        return timeToolTip(info);
    case EditHistoryModel::LinesColumn:
        return info.number == 0 ? QString()
                                : EditHistoryModel::tr("%1 modified, %2 added, %3 removed")
                                      .arg(info.modifiedLines).arg(info.addedLines).arg(info.removedLines);
    case EditHistoryModel::LineNumbersColumn:
        return info.number == 0 ? QString()
                                : EditHistoryModel::tr("Modified lines; +added lines; −removed lines.\n"
                                                       "Modified and added lines are numbered as after the step, removed ones as before it.\n"
                                                       "Double click to go to the line.");
    case EditHistoryModel::CharsColumn:
        return info.charsApproximate ? EditHistoryModel::tr("The texts were too long to compare character by character: the numbers are approximate")
                                     : QString();
    case EditHistoryModel::SavedColumn:
        return savedToolTip(info);
    }
    return {};
}

QString headerText(int section)
{
    switch (section)
    {
    case EditHistoryModel::NumberColumn:      return EditHistoryModel::tr("#");
    case EditHistoryModel::TimeColumn:        return EditHistoryModel::tr("Time");
    case EditHistoryModel::LinesColumn:       return EditHistoryModel::tr("Lines");
    case EditHistoryModel::LineNumbersColumn: return EditHistoryModel::tr("Line numbers");
    case EditHistoryModel::CharsColumn:       return EditHistoryModel::tr("Characters");
    case EditHistoryModel::SavedColumn:       return EditHistoryModel::tr("Saved");
    }
    return {};
}

QString headerToolTip(int section)
{
    switch (section)
    {
    case EditHistoryModel::NumberColumn:
        return EditHistoryModel::tr("Number of the state of the document after the step. 0 is the state it started in.\n"
                                    "Ctrl+Z moves the current state (▶) back by one, Ctrl+Shift+Z forward by one.");
    case EditHistoryModel::LinesColumn:
        return EditHistoryModel::tr("How many lines the step changed (modified, added and removed ones)");
    case EditHistoryModel::LineNumbersColumn:
        return EditHistoryModel::tr("Modified lines; +added lines; −removed lines.\n"
                                    "Modified and added lines are numbered as after the step, removed ones as before it.");
    case EditHistoryModel::CharsColumn:
        return EditHistoryModel::tr("Characters inserted (+) and removed (−), line breaks included");
    case EditHistoryModel::SavedColumn:
        return EditHistoryModel::tr("When the file was written while the document was in this state");
    }
    return {};
}

QVariant undoneRowBrush(const Info& info)
{
    if (info.applied)
        return {};
    return QBrush(QApplication::palette().color(QPalette::Disabled, QPalette::Text));
}

QFont rowFont(const Info& info)
{
    QFont font = QApplication::font();
    font.setBold(info.isCurrent);
    font.setItalic(!info.applied || (info.number > 0 && info.changedLines() == 0));
    return font;
}

QVariant currentRowBackground(const Info& info)
{
    if (!info.isCurrent)
        return {};
    QColor color = QApplication::palette().color(QPalette::Highlight);
    color.setAlpha(55);
    return QBrush(color);
}

int alignment(int column)
{
    switch (column)
    {
    case EditHistoryModel::NumberColumn:
    case EditHistoryModel::LinesColumn:
    case EditHistoryModel::TimeColumn:
    case EditHistoryModel::SavedColumn:
        return Qt::AlignCenter;
    case EditHistoryModel::CharsColumn:
        return Qt::AlignRight | Qt::AlignVCenter;
    }
    return Qt::AlignLeft | Qt::AlignVCenter;
}
} // namespace


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
        return headerText(section);
    if (role == Qt::ToolTipRole)
        return headerToolTip(section);
    return {};
}

QVariant EditHistoryModel::data(const QModelIndex& index, int role) const
{
    if (!history || !index.isValid() || index.row() > history->stepCount())
        return {};

    const Info info = history->stepInfo(index.row());
    switch (role)
    {
    case Qt::DisplayRole:                return displayText(info, index.column());
    case RichTextDelegate::RichTextRole: return richText(info, index.column());
    case Qt::ToolTipRole:                return toolTipText(info, index.column());
    case Qt::ForegroundRole:             return undoneRowBrush(info);
    case Qt::FontRole:                   return rowFont(info);
    case Qt::BackgroundRole:             return currentRowBackground(info);
    case Qt::TextAlignmentRole:          return alignment(index.column());
    }
    return {};
}


EditHistoryDialog::EditHistoryDialog(EditHistory* history, QWidget* parent)
    : QDialog(parent, Qt::Window), ui(new Ui::EditHistoryDialog), history(history), model(new EditHistoryModel(history, this))
{
    ui->setupUi(this);
    setUpTable();
    setUpDiffViewer();
    connectSignals();

    lastCurrent = history->currentIndex();
    updateSummary();
    selectStep(lastCurrent);
}

EditHistoryDialog::~EditHistoryDialog()
{
    delete ui;
}

void EditHistoryDialog::setUpTable()
{
    ui->table->setModel(model);
    ui->table->verticalHeader()->setDefaultSectionSize(QFontMetrics(ui->table->font()).height() + 10);

    for (const int column : { EditHistoryModel::LineNumbersColumn, EditHistoryModel::CharsColumn })
    {
        auto* delegate = new RichTextDelegate(false, ui->table);
        delegate->setPlainTextWhenSelected(true);
        ui->table->setItemDelegateForColumn(column, delegate);
    }

    QHeaderView* header = ui->table->horizontalHeader();
    for (int column = 0; column < EditHistoryModel::ColumnCount; ++column)
        header->setSectionResizeMode(column, column == EditHistoryModel::LineNumbersColumn ? QHeaderView::Stretch
                                                                                           : QHeaderView::ResizeToContents);
}

void EditHistoryDialog::setUpDiffViewer()
{
    ui->diffViewer->setRowActions(DiffViewerWidget::RowActions::None);
    ui->splitter->setStretchFactor(0, 3);
    ui->splitter->setStretchFactor(1, 2);
}

void EditHistoryDialog::connectSignals()
{
    connect(ui->table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this]() { showSelectedStep(); });
    connect(ui->table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) { requestJumpToStep(index.row()); });

    // The document keeps changing while this window is open: refresh, but not at every keystroke
    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    connect(&refreshTimer, &QTimer::timeout, this, &EditHistoryDialog::refresh);
    connect(history, &EditHistory::changed, &refreshTimer, qOverload<>(&QTimer::start));

    // Esc rejects the dialog, which only hides it: let it go for good, like the Close button does
    connect(this, &QDialog::finished, this, &QObject::deleteLater);
}

void EditHistoryDialog::requestJumpToStep(int number)
{
    if (!history || number < 1)
        return;
    const int line = history->stepLineInEditor(number);
    if (line >= 0)
        emit jumpToLineRequested(line);
}

int EditHistoryDialog::selectedStep() const
{
    const QModelIndex index = ui->table->currentIndex();
    return index.isValid() ? index.row() : -1;
}

void EditHistoryDialog::selectStep(int number)
{
    const QModelIndex index = model->index(number, 0);
    ui->table->setCurrentIndex(index);
    ui->table->selectRow(number);
    ui->table->scrollTo(index, QAbstractItemView::PositionAtCenter);
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
    selectStep(following ? lastCurrent : std::min(selected, history->stepCount()));
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
    text += QStringLiteral("  ·  ") + saveStatusText();

    const int discarded = history->discardedStepCount();
    if (discarded > 0)
        text += QStringLiteral("<br>") + tr("The text of the oldest %1 steps was dropped to save memory; their numbers are kept.").arg(discarded);

    ui->summaryLabel->setText(text);
}

QString EditHistoryDialog::saveStatusText() const
{
    const auto marks = history->saveMarks();
    if (marks.isEmpty())
        return tr("not saved in this session");

    const auto& last = marks.last();
    const QString time = EditHistoryFormat::time(last.time);
    if (last.stateDiscarded)
        return tr("last saved at %1 (that state is gone: edits were made after an undo)").arg(time);
    if (last.state == history->currentIndex())
        return tr("<b>saved</b> at %1 (the document is in the saved state)").arg(time);
    return tr("last saved at %1, in state %2").arg(time).arg(last.state);
}

void EditHistoryDialog::showSelectedStep()
{
    const int number = selectedStep();
    if (!history || number < 0)
    {
        ui->diffViewer->setDiffData({});
        ui->diffTitle->clear();
        return;
    }

    if (number == 0)
    {
        ui->diffViewer->setDiffData({});
        ui->diffTitle->setText(tr("The state the document started in (loaded at %1). Select a step to see what it changed.")
                                   .arg(EditHistoryFormat::time(history->baselineTime())));
        return;
    }

    ui->diffViewer->setDiffData(history->stepDiff(number));
    ui->diffTitle->setText(stepTitle(history->stepInfo(number)));
}

QString EditHistoryDialog::stepTitle(const EditHistory::StepInfo& info) const
{
    QString title = tr("Step %1, %2").arg(info.number).arg(EditHistoryFormat::time(info.time));
    if (!info.applied)
        title += tr(" (undone)");
    title += QStringLiteral(": ");

    if (info.detailsDiscarded)
        return title + tr("the text of this step was dropped to save memory.");
    if (info.changedLines() == 0)
        return title + tr("no text changed (only formatting).");
    return title + tr("%1 modified, %2 added, %3 removed lines; %4 characters")
                       .arg(info.modifiedLines).arg(info.addedLines).arg(info.removedLines).arg(EditHistoryFormat::charsPlain(info));
}
