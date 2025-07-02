#pragma once

#include <QStyledItemDelegate>
#include <QString>

/**
 * @brief The HighlightDelegate class
 * This class is to make search results more visible
 */
class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    HighlightDelegate(QObject *parent = nullptr);

    void setSearchTerm(const QString& term);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QString searchTerm;
};
