#pragma once

#include <QDialog>

namespace Ui
{
    class StcTablesCreator;
}

class StcTablesCreator : public QDialog
{
    Q_OBJECT

public:
    explicit StcTablesCreator(const QString& tableContent = QString(), QWidget *parent = nullptr);
    ~StcTablesCreator() override;

    QString generateTableContent() const;
    
    // Property getters
    bool hasHeader() const { return m_hasHeader; }
    bool isExtended() const { return m_isExtended; }
    
    // Property setters
    void setHeaderEnabled(bool enabled);
    void setExtendedEnabled(bool enabled);

private slots:
    void onAddColumnRight();
    void onAddRowBelow();
    void showRowContextMenu(const QPoint &pos);
    void showRowHeaderContextMenu(const QPoint &pos);
    void showColumnContextMenu(const QPoint &pos);
    void deleteSelectedRows();
    void insertRowAbove();
    void insertRowBelow();
    void deleteSelectedColumns();
    void insertColumnLeft();
    void insertColumnRight();

protected:
    void setupTable(const QString& content);
    void insertRowAt(int row);

private:
    Ui::StcTablesCreator *ui;

    bool m_hasHeader;
    bool m_isExtended;
    
    // Context menu actions
    QMenu *m_rowMenu = nullptr;
    QMenu *m_columnMenu = nullptr;
    QAction *m_deleteRowAction = nullptr;
    QAction *m_insertRowAboveAction = nullptr;
    QAction *m_insertRowBelowAction = nullptr;
    QAction *m_deleteColumnAction = nullptr;
    QAction *m_insertColumnLeftAction = nullptr;
    QAction *m_insertColumnRightAction = nullptr;
    
    // Current context menu position
    int m_contextMenuRow = -1;
    int m_contextMenuColumn = -1;
};
