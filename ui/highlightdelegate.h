#pragma once

#include <QList>
#include <QObject>
#include <QStringList>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QModelIndex;
class QPainter;

/**
 * @brief The HighlightDelegate class
 * This class is to make search results more visible
 */
class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    HighlightDelegate(QObject *parent = nullptr);

    /// The texts to make visible in the shown line: one for a plain search, a few for a search of words in any order
    void setSearchTerms(const QStringList& terms);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QStringList searchTerms;
};
