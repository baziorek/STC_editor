#include "LineHistoryDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

#include "EditHistoryFormat.h"
#include "RichTextDelegate.h"

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
    {
        switch (section)
        {
        case StepColumn:   return tr("Step");
        case TimeColumn:   return tr("Time");
        case ChangeColumn: return tr("Change");
        case SavedColumn:  return tr("Saved");
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case StepColumn:
            return tr("Number of the step in the table of edits. ▶ marks the state of the line in the document now.");
        case SavedColumn:
            return tr("When the file was first written with this change in it");
        }
    }
    return {};
}

QVariant LineHistoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= history.entries.size())
        return {};

    const EditHistory::LineEntry& entry = history.entries[index.row()];
    const bool isCurrent = index.row() == currentRow();

    switch (role)
    {
    case Qt::DisplayRole:
        switch (index.column())
        {
        case StepColumn:
            return isCurrent ? QStringLiteral("▶ %1").arg(entry.step) : QString::number(entry.step);
        case TimeColumn:
            return EditHistoryFormat::time(entry.time);
        case ChangeColumn:
            if (entry.detailsDiscarded)
                return tr("(the text was dropped to save memory)");
            return entry.kind == EditHistory::LineKind::Removed ? entry.oldText : entry.newText;
        case SavedColumn:
            return entry.savedAt.isValid() ? EditHistoryFormat::time(entry.savedAt) : QStringLiteral("–");
        }
        break;

    case RichTextDelegate::RichTextRole:
        if (index.column() == ChangeColumn)
        {
            if (entry.detailsDiscarded)
                return QStringLiteral("<i>") + tr("(the text was dropped to save memory)").toHtmlEscaped() + QStringLiteral("</i>");

            switch (entry.kind)
            {
            case EditHistory::LineKind::Modified:
                return EditHistoryFormat::inlineDiffHtml(entry.oldText, entry.newText);
            case EditHistory::LineKind::Added:
                return QStringLiteral("<i>%1</i> ").arg(tr("new line:").toHtmlEscaped()) + EditHistoryFormat::insertedHtml(entry.newText);
            case EditHistory::LineKind::Removed:
                return QStringLiteral("<i>%1</i> ").arg(tr("line removed:").toHtmlEscaped()) + EditHistoryFormat::deletedHtml(entry.oldText);
            }
        }
        break;

    case Qt::ToolTipRole:
        switch (index.column())
        {
        case StepColumn:
            if (isCurrent)
                return tr("The line is in this state now");
            return entry.applied ? QString() : tr("Undone: Ctrl+Shift+Z brings it back");
        case TimeColumn:
            return entry.time.toString(Qt::ISODate);
        case SavedColumn:
            return entry.savedAt.isValid() ? entry.savedAt.toString(Qt::ISODate) : tr("Not written to the file yet");
        case ChangeColumn:
            return tr("Green: added, red and struck through: removed. Double click to go to the line.");
        }
        break;

    case Qt::ForegroundRole:
        if (!entry.applied)
            return QBrush(QApplication::palette().color(QPalette::Disabled, QPalette::Text));
        break;

    case Qt::FontRole:
    {
        QFont font = QApplication::font();
        font.setBold(isCurrent);
        font.setItalic(!entry.applied);
        return font;
    }

    case Qt::BackgroundRole:
        if (isCurrent)
        {
            QColor color = QApplication::palette().color(QPalette::Highlight);
            color.setAlpha(55);
            return QBrush(color);
        }
        break;

    case Qt::TextAlignmentRole:
        if (index.column() == ChangeColumn)
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        return int(Qt::AlignCenter);
    }
    return {};
}


LineHistoryDialog::LineHistoryDialog(EditHistory* history, int lineId, QWidget* parent)
    : QDialog(parent, Qt::Window), history(history), lineId(lineId), model(new LineHistoryModel(this))
{
    resize(900, 420);

    summaryLabel = new QLabel(this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::RichText);

    table = new QTableView(this);
    table->setModel(model);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setWordWrap(true);
    table->verticalHeader()->hide();
    table->setItemDelegateForColumn(LineHistoryModel::ChangeColumn, new RichTextDelegate(true, table));

    QHeaderView* header = table->horizontalHeader();
    header->setSectionResizeMode(LineHistoryModel::StepColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(LineHistoryModel::TimeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(LineHistoryModel::ChangeColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(LineHistoryModel::SavedColumn, QHeaderView::ResizeToContents);
    // rows are as high as the wrapped text needs: again whenever the width of the text column changes
    connect(header, &QHeaderView::sectionResized, table, [this](int column) {
        if (column == LineHistoryModel::ChangeColumn)
            table->resizeRowsToContents();
    });

    auto* legend = new QLabel(QStringLiteral("<span style=\"color:#116329;background-color:#aceebb\">&nbsp;%1&nbsp;</span> "
                                             "<span style=\"color:#82071e;background-color:#ffc1ba;text-decoration:line-through\">&nbsp;%2&nbsp;</span>"
                                             "&nbsp;&nbsp;%3")
                                  .arg(tr("added").toHtmlEscaped(), tr("removed").toHtmlEscaped(),
                                       tr("Greyed entries are undone (Ctrl+Shift+Z brings them back); they disappear when you edit after an undo.").toHtmlEscaped()),
                              this);
    legend->setTextFormat(Qt::RichText);
    legend->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(summaryLabel);
    layout->addWidget(table, 1);
    layout->addWidget(legend);
    layout->addWidget(buttons);

    connect(table, &QTableView::doubleClicked, this, [this]() {
        if (!this->history)
            return;
        const int line = this->history->lineOfId(this->lineId);
        if (line >= 0)
            emit jumpToLineRequested(line);
    });

    refreshTimer.setSingleShot(true);
    refreshTimer.setInterval(150);
    connect(&refreshTimer, &QTimer::timeout, this, &LineHistoryDialog::refresh);
    connect(history, &EditHistory::changed, &refreshTimer, qOverload<>(&QTimer::start));

    refresh();
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
    table->resizeRowsToContents();
    updateSummary();

    const int row = model->currentRow();
    if (row >= 0)
    {
        table->selectRow(row);
        table->scrollTo(model->index(row, 0), QAbstractItemView::PositionAtCenter);
    }
    else
    {
        table->clearSelection();
    }
}

void LineHistoryDialog::updateSummary()
{
    const EditHistory::LineHistory& data = model->lineHistory();

    QString title;
    if (data.line >= 0)
    {
        setWindowTitle(tr("History of line %1").arg(data.line + 1));
        title = tr("Line <b>%1</b>").arg(data.line + 1);
    }
    else
    {
        setWindowTitle(tr("History of a line"));
        title = data.entries.isEmpty() ? tr("This line has no history any more (the document was replaced).")
                                       : tr("This line is not in the document now (its creation is undone).");
    }

    if (data.entries.isEmpty())
    {
        summaryLabel->setText(title);
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
    summaryLabel->setText(text);
}
