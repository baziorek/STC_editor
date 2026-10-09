#pragma once

#include <QAbstractTableModel>
#include <QDialog>
#include <QPointer>
#include <QTimer>

#include "utils/EditHistory.h"

namespace Ui {
class EditHistoryDialog;
}

/// The rows of the table of edits: row 0 is the state the document started in, row N is the N-th step
class EditHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        NumberColumn,
        TimeColumn,
        LinesColumn,
        LineNumbersColumn,
        CharsColumn,
        SavedColumn,
        ColumnCount
    };

    explicit EditHistoryModel(EditHistory* history, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void refresh();

private:
    QPointer<EditHistory> history;
};

/// "Edit history": every undo step of the document in a table (when, which lines, how many characters, whether and
/// when the file was saved, where the current state is) and what the selected step changed.
/// It follows the document while it is open. The layout is in EditHistoryDialog.ui.
class EditHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditHistoryDialog(EditHistory* history, QWidget* parent = nullptr);
    ~EditHistoryDialog() override;

signals:
    /// A step was double clicked: zero-based number of the line to show in the editor
    void jumpToLineRequested(int line);

private:
    void setUpTable();
    void setUpDiffViewer();
    void connectSignals();

    void refresh();
    void updateSummary();
    QString saveStatusText() const;
    void showSelectedStep();
    QString stepTitle(const EditHistory::StepInfo& info) const;
    void requestJumpToStep(int number);
    int selectedStep() const;
    void selectStep(int number);

    Ui::EditHistoryDialog* ui;
    QPointer<EditHistory> history;
    EditHistoryModel* model;
    QTimer refreshTimer;
    int lastCurrent = 0;
};
