
#pragma once

#include <QDialog>

class QTableWidgetItem;

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
    void showCellContextMenu(const QPoint &pos);
    void deleteSelectedRows();
    void insertRowAbove();
    void insertRowBelow();
    void deleteSelectedColumns();
    void insertColumnLeft();
    void insertColumnRight();
    void onHeaderDoubleClicked(int logicalIndex);

    // Drag & Drop functionality
    void onItemChanged(QTableWidgetItem* item);

    // Cell clipboard operations
    void copyCellContent();
    void pasteCellContent();

protected:
    void accept() override;
    void setupTable(const QString& content);
    void insertRowAt(int row);
    QString generateTableContentImpl() const;

    // Event filters for drag & drop
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    // UI initialization methods
    void initializeUI();
    void setupTableWidget();
    void setupContextMenus();
    void createRowContextMenu();
    void createColumnContextMenu();
    void setupButtons();
    void setupCheckBoxes();
    void connectSignalsAndSlots();
    void performPaste(const QString& content);

    // Table setup helper methods
    void clearTableWidget();
    void parseCsvAttributes(const QString& content);
    QString normalizeLineEndings(const QString& content);
    QStringList splitContentIntoLines(const QString& normalizedContent);
    QStringList processAndTrimLines(const QStringList& rawLines);
    bool detectHeaderFromFirstLine(const QStringList& lines);
    int calculateMaxColumnCount(const QStringList& lines);
    void configureTableDimensions(int maxColumns, int rowCount);
    void setupTableHeaders(const QStringList& lines, int maxColumns);
    void populateTableWithData(const QStringList& dataLines, int maxColumns);
    void finalizeTableAppearance();

    // CSV parsing helper methods
    QStringList parseLineIntoCells(const QString& line);
    int countColumnsInLine(const QString& line);

    void setupDragAndDrop();
    void startCustomDrag();
    void finishCustomDrag(const QPoint& dropPosition);
    void performCellSwap(int targetRow, int targetColumn);
    void resetDragState();

    // Cell clipboard helper methods
    void showPasteConfirmationDialog(const QString& currentContent, const QString& clipboardContent);

    // Context menu setup
    void createCellContextMenu();

private:
    Ui::StcTablesCreator *ui;
    QString m_generatedContent;
    bool m_hasHeader = false;
    bool m_isExtended = false;

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

    // Drag & Drop state
    bool m_isDragging = false;
    QPoint m_dragStartPosition;
    int m_dragSourceRow = -1;
    int m_dragSourceColumn = -1;
    QString m_draggedContent;

    // Cell context menu and clipboard
    QMenu *m_cellMenu = nullptr;
    QAction *m_copyCellAction = nullptr;
    QAction *m_pasteCellAction = nullptr;

    // Current context menu position for cells
    int m_contextCellRow = -1;
    int m_contextCellColumn = -1;

};
