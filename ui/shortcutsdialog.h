#pragma once

#include <QDialog>
#include <QKeySequence>
#include <QMultiMap>

QT_BEGIN_NAMESPACE
class QTableWidget;
QT_END_NAMESPACE

class ShortcutsDialog : public QDialog
{
    Q_OBJECT

public:
    ShortcutsDialog(QWidget *parent = nullptr);

    void addShortcuts(const QMultiMap<QString, QKeySequence> &shortcuts, const QString &category);
    void addQActions(const QList<QAction *> &actions, const QString &category);

private:
    QTableWidget *table;
};
