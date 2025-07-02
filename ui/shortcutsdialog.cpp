#include "shortcutsdialog.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>
#include <QAction>


ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Available Shortcuts");

    auto layout = new QVBoxLayout(this);
    table = new QTableWidget(this);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({ "Category", "Action", "Shortcut" });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(table);

    auto closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &ShortcutsDialog::accept);
    layout->addWidget(closeBtn);
}

void ShortcutsDialog::addShortcuts(const QMultiMap<QString, QKeySequence> &shortcuts, const QString &category)
{
    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        const QString &description = it.key();
        const QString shortcutStr = it.value().toString();

        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(category));
        table->setItem(row, 1, new QTableWidgetItem(description));
        table->setItem(row, 2, new QTableWidgetItem(shortcutStr));
    }

    table->resizeColumnsToContents();
    table->resizeRowsToContents();
}

void ShortcutsDialog::addQActions(const QList<QAction *> &actions, const QString &category)
{
    for (QAction *action : actions) {
        if (!action->shortcut().isEmpty()) {
            int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(category));
            table->setItem(row, 1, new QTableWidgetItem(action->text()));
            table->setItem(row, 2, new QTableWidgetItem(action->shortcut().toString()));
        }
    }

    table->resizeColumnsToContents();
    table->resizeRowsToContents();
}

void ShortcutsDialog::adjustSizeToContents()
{
    table->resizeColumnsToContents();
    table->resizeRowsToContents();

    int totalWidth = table->verticalHeader()->width(); // rows number
    for (int col = 0; col < table->columnCount(); ++col)
        totalWidth += table->columnWidth(col);

    int totalHeight = table->horizontalHeader()->height(); // column header
    int visibleRows = std::min(table->rowCount(), 20); // limit heigh
    for (int row = 0; row < visibleRows; ++row)
        totalHeight += table->rowHeight(row);

    // add margin
    totalWidth += 40;
    totalHeight += 80;

    resize(totalWidth, totalHeight);
}
