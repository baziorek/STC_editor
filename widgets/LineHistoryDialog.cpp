#include "LineHistoryDialog.h"

#include <QApplication>
#include <QHeaderView>

#include "EditHistoryFormat.h"
#include "RichTextDelegate.h"
#include "ui_LineHistoryDialog.h"

namespace
{
using Entry = EditHistory::LineEntry;

QString displayText(const Entry& entry, bool isCurrent, int column)
{
    switch (column)
    {
    case LineHistoryModel::StepColumn:
        return isCurrent ? QStringLiteral("▶ %1").arg(entry.step) : QString::number(entry.step);
    case LineHistoryModel::TimeColumn:
        return EditHistoryFormat::time(entry.time);
    case LineHistoryModel::ChangeColumn:
        if (entry.detailsDiscarded)
            return LineHistoryModel::tr("(the text was dropped to save memory)");
        return entry.kind == EditHistory::LineKind::Removed ? entry.oldText : entry.newText;
    case LineHistoryModel::SavedColumn:
        return entry.savedAt.isValid() ? EditHistoryFormat::time(entry.savedAt) : QStringLiteral("–");
    }
    return {};
}

/// The change in one line, coloured
QVariant richText(const Entry& entry, int column)
{
    if (column != LineHistoryModel::ChangeColumn)
        return {};

    if (entry.detailsDiscarded)
        return QStringLiteral("<i>") + LineHistoryModel::tr("(the text was dropped to save memory)").toHtmlEscaped() + QStringLiteral("</i>");

    switch (entry.kind)
    {
    case EditHistory::LineKind::Modified:
        return EditHistoryFormat::inlineDiffHtml(entry.oldText, entry.newText);
    case EditHistory::LineKind::Added:
        return QStringLiteral("<i>%1</i> ").arg(LineHistoryModel::tr("new line:").toHtmlEscaped()) + EditHistoryFormat::insertedHtml(entry.newText);
    case EditHistory::LineKind::Removed:
        return QStringLiteral("<i>%1</i> ").arg(LineHistoryModel::tr("line removed:").toHtmlEscaped()) + EditHistoryFormat::deletedHtml(entry.oldText);
    }
    return {};
}

QString toolTipText(const Entry& entry, bool isCurrent, int column)
{
    switch (column)
    {
    case LineHistoryModel::StepColumn:
        if (isCurrent)
            return LineHistoryModel::tr("The line is in this state now");
        return entry.applied ? QString() : LineHistoryModel::tr("Undone: Ctrl+Shift+Z brings it back");
    case LineHistoryModel::TimeColumn:
        return entry.time.toString(Qt::ISODate);
    case LineHistoryModel::SavedColumn:
        return entry.savedAt.isValid() ? entry.savedAt.toString(Qt::ISODate) : LineHistoryModel::tr("Not written to the file yet");
    case LineHistoryModel::ChangeColumn:
        return LineHistoryModel::tr("Green: added, red and struck through: removed. Double click to go to the line.");
    }
    return {};
}

QString headerText(int section)
{
    switch (section)
    {
    case LineHistoryModel::StepColumn:   return LineHistoryModel::tr("Step");
    case LineHistoryModel::TimeColumn:   return LineHistoryModel::tr("Time");
    case LineHistoryModel::ChangeColumn: return LineHistoryModel::tr("Change");
    case LineHistoryModel::SavedColumn:  return LineHistoryModel::tr("Saved");
    }
    return {};
}

QString headerToolTip(int section)
{
    switch (section)
    {
    case LineHistoryModel::StepColumn:
        return LineHistoryModel::tr("Number of the step in the table of edits. ▶ marks the state of the line in the document now.");
    case LineHistoryModel::SavedColumn:
        return LineHistoryModel::tr("When the file was first written with this change in it");
    }
    return {};
}

QFont rowFont(const Entry& entry, bool isCurrent)
{
    QFont font = QApplication::font();
    font.setBold(isCurrent);
    font.setItalic(!entry.applied);
    return font;
}

QVariant currentRowBackground(bool isCurrent)
{
    if (!isCurrent)
        return {};
    QColor color = QApplication::palette().color(QPalette::Highlight);
    color.setAlpha(55);
    return QBrush(color);
}
} // namespace


LineHistoryModel::LineHistoryModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void LineHistoryModel::setLineHistory(const EditHistory::LineHistory& newHistory)
{
    beginResetModel();
    history = newHistory;
    endResetModel();
}

int LineHistoryModel::currentRow() const
{
    int row = -1;
    for (int i = 0; i < history.entries.size(); ++i)
    {
        if (history.entries[i].applied)
            row = i;
    }
    return row;
}

int LineHistoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(history.entries.size());
}

int LineHistoryModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LineHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::DisplayRole)
        return headerText(section);
    if (role == Qt::ToolTipRole)
        return headerToolTip(section);
    return {};
}

QVariant LineHistoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= history.entries.size())
        return {};

    const Entry& entry = history.entries[index.row()];
    const bool isCurrent = index.row() == currentRow();

    switch (role)
    {
    case Qt::DisplayRole:                return displayText(entry, isCurrent, index.column());
    case RichTextDelegate::RichTextRole: return richText(entry, index.column());
    case Qt::ToolTipRole:                return toolTipText(entry, isCurrent, index.column());
    case Qt::ForegroundRole:             return entry.applied ? QVariant() : QVariant(QBrush(QApplication::palette().color(QPalette::Disabled, QPalette::Text)));
    case Qt::FontRole:                   return rowFont(entry, isCurrent);
    case Qt::BackgroundRole:             return currentRowBackground(isCurrent);
    case Qt::TextAlignmentRole:          return int(index.column() == ChangeColumn ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignCenter);
    }
    return {};
}


LineHistoryDialog::LineHistoryDialog(EditHistory* history, int lineId, QWidget* parent)
    : QDialog(parent, Qt::Window), ui(new Ui::LineHistoryDialog), history(history), lineId(lineId), model(new LineHistoryModel(this))
{
    ui->setupUi(this);
    setUpTable();
    connectSignals();
    refresh();
}

LineHistoryDialog::~LineHistoryDialog()
{
    delete ui;
}

void LineHistoryDialog::setUpTable()
{
    ui->table->setModel(model);
    ui->table->setItemDelegateForColumn(LineHistoryModel::ChangeColumn, new RichTextDelegate(true, ui->table));

    QHeaderView* header = ui->table->horizontalHeader();
    for (int column = 0; column < LineHistoryModel::ColumnCount; ++column)
        header->setSectionResizeMode(column, column == LineHistoryModel::ChangeColumn ? QHeaderView::Stretch
                                                                                      : QHeaderView::ResizeToContents);

    // rows are as high as the wrapped text needs: again whenever the width of the text column changes
    connect(header, &QHeaderView::sectionResized, ui->table, [this](int column) {
        if (column == LineHistoryModel::ChangeColumn)
            ui->table->resizeRowsToContents();
    });
}

void LineHistoryDialog::connectSignals()
{
    connect(ui->table, &QTableView::doubleClicked, this, [this]() { requestJumpToLine(); });

    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    connect(&refreshTimer, &QTimer::timeout, this, &LineHistoryDialog::refresh);
    connect(history, &EditHistory::changed, &refreshTimer, qOverload<>(&QTimer::start));

    // Esc rejects the dialog, which only hides it: let it go for good, like the Close button does
    connect(this, &QDialog::finished, this, &QObject::deleteLater);
}

void LineHistoryDialog::requestJumpToLine()
{
    if (!history)
        return;
    const int line = history->lineOfId(lineId);
    if (line >= 0)
        emit jumpToLineRequested(line);
}

void LineHistoryDialog::showLine(int newLineId)
{
    lineId = newLineId;
    refresh();
}

void LineHistoryDialog::refresh()
{
    if (!history)
        return;

    model->setLineHistory(history->lineHistoryById(lineId));
    ui->table->resizeRowsToContents();
    updateSummary();

    const int row = model->currentRow();
    if (row < 0)
    {
        ui->table->clearSelection();
        return;
    }
    ui->table->selectRow(row);
    ui->table->scrollTo(model->index(row, 0), QAbstractItemView::PositionAtCenter);
}

QString LineHistoryDialog::windowTitleText() const
{
    const int line = model->lineHistory().line;
    return line >= 0 ? tr("History of line %1").arg(line + 1) : tr("History of a line");
}

QString LineHistoryDialog::lineTitle() const
{
    const EditHistory::LineHistory& data = model->lineHistory();
    if (data.line >= 0)
        return tr("Line <b>%1</b>").arg(data.line + 1);
    return data.entries.isEmpty() ? tr("This line has no history any more (the document was replaced).")
                                  : tr("This line is not in the document now (its creation is undone).");
}

void LineHistoryDialog::updateSummary()
{
    const EditHistory::LineHistory& data = model->lineHistory();
    setWindowTitle(windowTitleText());
    const QString title = lineTitle();
    if (data.entries.isEmpty())
    {
        ui->summaryLabel->setText(title);
        return;
    }

    int undone = 0;
    for (const auto& entry : data.entries)
        undone += entry.applied ? 0 : 1;

    QString text = title + QStringLiteral(": ") + tr("%n change(s) in this session", "", static_cast<int>(data.entries.size()));
    if (undone > 0)
        text += tr(", %1 of them undone").arg(undone);
    text += QStringLiteral("  ·  ") + tr("current state of the document: step <b>%1</b> of %2").arg(data.currentIndex).arg(data.stepCount);
    if (model->currentRow() < 0)
        text += QStringLiteral("  ·  ") + tr("the line is in the state before the first change");
    ui->summaryLabel->setText(text);
}
