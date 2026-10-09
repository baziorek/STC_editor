#pragma once

#include <QAbstractTableModel>
#include <QDialog>
#include <QPointer>
#include <QTimer>

#include "utils/EditHistory.h"

namespace Ui {
class LineHistoryDialog;
}

/// The entries of the history of one line, oldest first
class LineHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        StepColumn,
        TimeColumn,
        ChangeColumn,
        SavedColumn,
        ColumnCount
    };

    explicit LineHistoryModel(QObject* parent = nullptr);

    void setLineHistory(const EditHistory::LineHistory& history);
    const EditHistory::LineHistory& lineHistory() const { return history; }

    /// The entry which is in the document now: the last one which is not undone; -1 when all of them are undone
    int currentRow() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    EditHistory::LineHistory history;
};

/// What happened to one line during this session: when it was changed, what exactly (green: added, red: removed),
/// when the change was written to the file, and where the current state of the document is.
/// It follows the line while it is open (the line is found by its identity, not by its number).
/// The layout is in LineHistoryDialog.ui.
class LineHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    LineHistoryDialog(EditHistory* history, int lineId, QWidget* parent = nullptr);
    ~LineHistoryDialog() override;

    /// Show another line in the same window
    void showLine(int lineId);

signals:
    /// An entry was double clicked: zero-based number of the line to show in the editor
    void jumpToLineRequested(int line);

private:
    void setUpTable();
    void connectSignals();

    void refresh();
    void updateSummary();
    /// "Line 12", or why there is no line
    QString lineTitle() const;
    QString windowTitleText() const;
    void requestJumpToLine();

    Ui::LineHistoryDialog* ui;
    QPointer<EditHistory> history;
    int lineId;
    LineHistoryModel* model;
    QTimer refreshTimer;
};
