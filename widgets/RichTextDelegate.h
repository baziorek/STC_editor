#pragma once

#include <QStyledItemDelegate>

/// Draws the cells whose model gives HTML under RichTextRole (the other cells are drawn as usual).
/// With `wrap` the text breaks into lines at the width of the column and the rows get as high as the text needs.
class RichTextDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int RichTextRole = Qt::UserRole + 1;

    explicit RichTextDelegate(bool wrap, QObject* parent = nullptr);

    /// For text which is only coloured (no background of its own): on a selected row it is drawn as plain text, in the
    /// colour of selected text, because green or red on the blue selection cannot be read
    void setPlainTextWhenSelected(bool plain) { plainWhenSelected = plain; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    bool wrap;
    bool plainWhenSelected = false;
};
