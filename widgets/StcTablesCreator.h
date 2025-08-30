#pragma once

#include <QDialog>

// Forward declarations
class QTableWidget;
class QTableWidgetItem;

namespace Ui {
class StcTablesCreator;
}

class StcTablesCreator : public QDialog
{
    Q_OBJECT

public:
    explicit StcTablesCreator(const QString& tableContent = QString(), QWidget *parent = nullptr);
    ~StcTablesCreator() override;

    QString getTableContent() const;

private slots:
    void onAddColumnRight();
    void onAddRowBelow();
    void onAccepted();

private:
    void setupTable(const QString& content);
    QString generateTableContent() const;

    Ui::StcTablesCreator *ui;
    bool m_hasHeader;
    bool m_isExtended;
};
