#pragma once

#include <memory>
#include <QHash>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>

class QPlainTextEdit;
class QTextDocument;

/// Helpers for the "is the cell formatted (`[run]...[/run]`)?" model used by the table editor.
/// The cell keeps only the inner text; `[run]` wrapper is represented by the check state and is
/// added back when the table is turned into STC.
namespace stc::tablecell
{
struct Decoded
{
    QString inner;
    bool formatted = false;
};

/// True when the whole text is exactly one `[run]...[/run]` pair (no other `[run]` / `[/run]` inside).
bool isWrappedInRun(const QString& text);

/// `[run]x[/run]` -> {x, true}; anything else -> {text, false}.
Decoded decode(const QString& stc);

/// {x, true} -> `[run]x[/run]` (empty text is never wrapped); {x, false} -> x with line breaks replaced
/// by spaces (outside of `[run]` a line break would start a new table row).
QString encode(const QString& inner, bool formatted);

/// Plain (not wrapped) text which still contains `[run]` or `[/run]` somewhere inside.
bool hasRunInsideUnwrapped(const QString& inner, bool formatted);

/// Number of `;` which would be taken as cell separators (not inside quotes and not inside `[...]`).
int separatorsInText(const QString& text);
}

/// Table cell with the check box showing whether the cell is formatted.
class StcCellItem : public QTableWidgetItem
{
public:
    explicit StcCellItem(const QString& text = {}, bool formatted = false);

    QTableWidgetItem* clone() const override;

    bool isFormatted() const
    {
        return checkState() == Qt::Checked;
    }
    void setFormatted(bool formatted)
    {
        setCheckState(formatted ? Qt::Checked : Qt::Unchecked);
    }
};

/// Formatted cells are painted and edited with the STC syntax highlighting, the plain ones are
/// ordinary plain text. Enter accepts the edit, Shift+Enter / Ctrl+Enter inserts a new line
/// (formatted cells only - a new line in a plain cell would break the row).
class StcCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit StcCellDelegate(QObject* parent = nullptr);
    ~StcCellDelegate() override;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

signals:
    /// Emitted when an editor of a cell is opened; the palette of the tags works on it.
    void cellEditorCreated(QPlainTextEdit* editor, bool formatted);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    QTextDocument& documentFor(const QString& text, const QFont& font, int width) const;
    int textWidthOf(const QStyleOptionViewItem& option, const QModelIndex& index) const;

    mutable QHash<QString, std::shared_ptr<QTextDocument>> m_documents;
};
