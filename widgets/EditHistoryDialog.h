#pragma once

#include <QAbstractTableModel>
#include <QDialog>
#include <QPointer>
#include <QTimer>

#include "utils/EditHistory.h"

class DiffViewerWidget;
class QLabel;
class QTableView;

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
/// It follows the document while it is open.
class EditHistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditHistoryDialog(EditHistory* history, QWidget* parent = nullptr);

signals:
    /// A step was double clicked: zero-based number of the line to show in the editor
    void jumpToLineRequested(int line);

private:
    void refresh();
    void updateSummary();
    void showSelectedStep();
    int selectedStep() const;
    void selectStep(int number);

    QPointer<EditHistory> history;
    EditHistoryModel* model;
    QTableView* table;
    QLabel* summaryLabel;
    QLabel* diffTitle;
    DiffViewerWidget* diffViewer;
    QTimer refreshTimer;
    int lastCurrent = 0;
};
