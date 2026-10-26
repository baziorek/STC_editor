#include <QDialogButtonBox>
#include <QMessageBox>
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMouseEvent>
#include <QClipboard>
#include <QTimer>
#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QChar>
#include <QCheckBox>
#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>
#include <Qt>
#include <QtGlobal>

#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QToolButton>
#include "StcTablesCreator.h"
#include "StcCellDelegate.h"
#include "StcInlineTagToggle.h"
#include "ui/stctagsbuttons.h"
#include "ui_StcTablesCreator.h"

class QAbstractButton;


StcTablesCreator::StcTablesCreator(const QString& tableContent, QWidget *parent)
    : QDialog(parent), ui(new Ui::StcTablesCreator)
{
    initializeUI();

    // Load content if provided
    if (!tableContent.isEmpty())
    {
        setupTable(tableContent);
    }
}

void StcTablesCreator::initializeUI()
{
    ui->setupUi(this);
    setWindowTitle(tr("Table Editor"));

    setupTableWidget();
    setupContextMenus();
    setupButtons();
    setupCheckBoxes();
    connectSignalsAndSlots();
    setupCellEditing();
}

void StcTablesCreator::setupTableWidget()
{
    // Configure table with default dimensions and column stretching
    ui->tableWidget->setColumnCount(2);
    ui->tableWidget->setRowCount(2);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Enable word wrapping in table cells
    ui->tableWidget->setWordWrap(true);

    // Set automatic row height adjustment to accommodate wrapped text
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    // Enable custom context menus for ALL parts of the table
    ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableWidget->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableWidget->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    // Setup drag and drop functionality
    setupDragAndDrop();
}

void StcTablesCreator::setupDragAndDrop()
{
    // disable build-in drag&drop
    ui->tableWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    ui->tableWidget->setDragDropOverwriteMode(false);

    // Install event filter to handle mouse events manually
    ui->tableWidget->installEventFilter(this);
    ui->tableWidget->viewport()->installEventFilter(this);

    // Connect item selection changes
    connect(ui->tableWidget, &QTableWidget::itemChanged, this, &StcTablesCreator::onItemChanged);
}

bool StcTablesCreator::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->tableWidget || obj == ui->tableWidget->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                m_dragStartPosition = mouseEvent->pos();

                // Find which item was clicked and store its content
                QTableWidgetItem* item = ui->tableWidget->itemAt(m_dragStartPosition);
                if (item)
                {
                    m_dragSourceRow = item->row();
                    m_dragSourceColumn = item->column();
                    m_draggedContent = cellToStc(m_dragSourceRow, m_dragSourceColumn);
                    m_isDragging = false;

                    qDebug() << "Mouse press - stored content:" << m_draggedContent
                             << "from cell (" << m_dragSourceRow << "," << m_dragSourceColumn << ")";
                }
                return false; // Let Qt handle the normal click
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) && !m_isDragging &&
                m_dragSourceRow != -1 && m_dragSourceColumn != -1)
            {

                int distance = (mouseEvent->pos() - m_dragStartPosition).manhattanLength();
                if (distance >= QApplication::startDragDistance())
                {
                    startCustomDrag();
                    m_isDragging = true;
                    return true; // Block further processing
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_isDragging) {
                finishCustomDrag(mouseEvent->pos());
                return true; // Block further processing
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void StcTablesCreator::startCustomDrag()
{
    qDebug() << "Starting custom drag with content:" << m_draggedContent
             << "from cell (" << m_dragSourceRow << "," << m_dragSourceColumn << ")";

    // Change cursor to indicate dragging
    ui->tableWidget->setCursor(Qt::ClosedHandCursor);
}

void StcTablesCreator::finishCustomDrag(const QPoint& dropPosition)
{
    qDebug() << "Finishing custom drag at position:" << dropPosition;

    // Restore cursor
    ui->tableWidget->setCursor(Qt::ArrowCursor);

    // Find target item
    QTableWidgetItem* targetItem = ui->tableWidget->itemAt(dropPosition);
    if (!targetItem)
    {
        qDebug() << "No target item found at drop position";
        resetDragState();
        return;
    }

    int targetRow = targetItem->row();
    int targetColumn = targetItem->column();

    // Don't swap with the same cell
    if (targetRow == m_dragSourceRow && targetColumn == m_dragSourceColumn)
    {
        qDebug() << "Dropped on the same cell - no action needed";
        resetDragState();
        return;
    }

    // Perform the swap using our manually stored content
    performCellSwap(targetRow, targetColumn);

    resetDragState();
}

void StcTablesCreator::performCellSwap(int targetRow, int targetColumn)
{
    // The STC form carries the "formatted" flag too, so it travels with the text
    const QString targetContent = cellToStc(targetRow, targetColumn);
    const QString sourceContent = m_draggedContent;

    qDebug() << "Performing swap:"
             << QString("Source (%1,%2): '%3'").arg(m_dragSourceRow).arg(m_dragSourceColumn).arg(sourceContent)
             << QString("Target (%1,%2): '%3'").arg(targetRow).arg(targetColumn).arg(targetContent);

    setCellFromStc(m_dragSourceRow, m_dragSourceColumn, targetContent);
    setCellFromStc(targetRow, targetColumn, sourceContent);
}

void StcTablesCreator::resetDragState()
{
    m_isDragging = false;
    m_dragSourceRow = -1;
    m_dragSourceColumn = -1;
    m_draggedContent.clear();
    m_dragStartPosition = QPoint();
}

void StcTablesCreator::onItemChanged(QTableWidgetItem* item)
{
    if (!item)
    {
        return;
    }
    {
        QSignalBlocker blocker(ui->tableWidget);
        if (stc::tablecell::isWrappedInRun(item->text()))
        {
            // typed or pasted `[run]...[/run]` around the whole cell means "formatted cell"
            item->setText(stc::tablecell::decode(item->text()).inner);
            item->setCheckState(Qt::Checked);
        }
        refreshItemDiagnostics(item);
    }
    updatePaletteState();
}

void StcTablesCreator::setupContextMenus()
{
    createRowContextMenu();
    createColumnContextMenu();
    createCellContextMenu();

    // Initialize context menu position tracking
    m_contextMenuRow = -1;
    m_contextMenuColumn = -1;
}

void StcTablesCreator::createRowContextMenu()
{
    // Create row context menu with actions
    m_rowMenu = new QMenu(this);

    // Create row actions with appropriate icons and text
    m_deleteRowAction = new QAction(QIcon::fromTheme("edit-delete"), tr("Delete Row"), this);
    m_insertRowAboveAction = new QAction(QIcon::fromTheme("list-add"), tr("Insert Row Above"), this);
    m_insertRowBelowAction = new QAction(QIcon::fromTheme("list-add"), tr("Insert Row Below"), this);

    // Add actions to menu with logical grouping
    m_rowMenu->addAction(m_insertRowAboveAction);
    m_rowMenu->addAction(m_insertRowBelowAction);
    m_rowMenu->addSeparator();
    m_rowMenu->addAction(m_deleteRowAction);
}

void StcTablesCreator::createColumnContextMenu()
{
    // Create column context menu with actions
    m_columnMenu = new QMenu(this);

    // Create column actions with appropriate icons and text
    m_deleteColumnAction = new QAction(QIcon::fromTheme("edit-delete"), tr("Delete Column"), this);
    m_insertColumnLeftAction = new QAction(QIcon::fromTheme("list-add"), tr("Insert Column Left"), this);
    m_insertColumnRightAction = new QAction(QIcon::fromTheme("list-add"), tr("Insert Column Right"), this);

    // Add actions to menu with logical grouping
    m_columnMenu->addAction(m_insertColumnLeftAction);
    m_columnMenu->addAction(m_insertColumnRightAction);
    m_columnMenu->addSeparator();
    m_columnMenu->addAction(m_deleteColumnAction);
}

void StcTablesCreator::setupButtons()
{
    // Set icons for main action buttons
    ui->addColumnButton->setIcon(QIcon::fromTheme("list-add"));
    ui->addRowButton->setIcon(QIcon::fromTheme("list-add"));
}

void StcTablesCreator::setupCheckBoxes()
{
    // Initialize checkbox states based on member variables
    ui->headerCheckBox->setChecked(m_hasHeader);
    ui->extendedCheckBox->setChecked(m_isExtended);
}

void StcTablesCreator::connectSignalsAndSlots()
{
    // Connect table widget context menus
    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, this, &StcTablesCreator::showRowContextMenu);
    connect(ui->tableWidget->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &StcTablesCreator::showColumnContextMenu);
    connect(ui->tableWidget->verticalHeader(), &QHeaderView::customContextMenuRequested, this, &StcTablesCreator::showRowHeaderContextMenu);

    // Connect row context menu actions
    connect(m_deleteRowAction, &QAction::triggered, this, &StcTablesCreator::deleteSelectedRows);
    connect(m_insertRowAboveAction, &QAction::triggered, this, &StcTablesCreator::insertRowAbove);
    connect(m_insertRowBelowAction, &QAction::triggered, this, &StcTablesCreator::insertRowBelow);

    // Connect column context menu actions
    connect(m_deleteColumnAction, &QAction::triggered, this, &StcTablesCreator::deleteSelectedColumns);
    connect(m_insertColumnLeftAction, &QAction::triggered, this, &StcTablesCreator::insertColumnLeft);
    connect(m_insertColumnRightAction, &QAction::triggered, this, &StcTablesCreator::insertColumnRight);

    // Connect main action buttons
    connect(ui->addColumnButton, &QPushButton::clicked, this, &StcTablesCreator::onAddColumnRight);
    connect(ui->addRowButton, &QPushButton::clicked, this, &StcTablesCreator::onAddRowBelow);

    // Connect checkboxes with lambda functions for state updates
    connect(ui->headerCheckBox, &QCheckBox::toggled, this, [this](bool checked) { m_hasHeader = checked; applyHeaderStyle(); });
    connect(ui->extendedCheckBox, &QCheckBox::toggled, this, [this](bool checked) { m_isExtended = checked; });

    // Connect dialog buttons
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &StcTablesCreator::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, this, &StcTablesCreator::showCellContextMenu);
}

void StcTablesCreator::createCellContextMenu()
{
    m_cellMenu = new QMenu(this);

    // Copy cell action
    m_copyCellAction = new QAction(tr("Kopiuj komórkę"), this);
    m_copyCellAction->setIcon(QIcon(":/icons/copy")); // Jeśli masz ikonę
    connect(m_copyCellAction, &QAction::triggered, this, &StcTablesCreator::copyCellContent);
    m_cellMenu->addAction(m_copyCellAction);

    // Paste cell action
    m_pasteCellAction = new QAction(tr("Wklej do komórki"), this);
    m_pasteCellAction->setIcon(QIcon(":/icons/paste")); // Jeśli masz ikonę
    connect(m_pasteCellAction, &QAction::triggered, this, &StcTablesCreator::pasteCellContent);
    m_cellMenu->addAction(m_pasteCellAction);

    m_cellMenu->addSeparator();

    // Add existing row/column actions to cell menu for convenience
    m_cellMenu->addAction(m_insertRowAboveAction);
    m_cellMenu->addAction(m_insertRowBelowAction);
    m_cellMenu->addAction(m_deleteRowAction);
    m_cellMenu->addSeparator();
    m_cellMenu->addAction(m_insertColumnLeftAction);
    m_cellMenu->addAction(m_insertColumnRightAction);
    m_cellMenu->addAction(m_deleteColumnAction);
}

void StcTablesCreator::showCellContextMenu(const QPoint &pos)
{
    QTableWidgetItem* item = ui->tableWidget->itemAt(pos);
    if (!item) {
        // If clicked on empty space, still allow context menu but create item
        QModelIndex index = ui->tableWidget->indexAt(pos);
        if (index.isValid()) {
            m_contextCellRow = index.row();
            m_contextCellColumn = index.column();
        } else {
            return; // Clicked outside table
        }
    } else {
        m_contextCellRow = item->row();
        m_contextCellColumn = item->column();
    }

    // Update context menu row/column for existing actions
    m_contextMenuRow = m_contextCellRow;
    m_contextMenuColumn = m_contextCellColumn;

    // Update paste action availability based on clipboard content
    QClipboard* clipboard = QApplication::clipboard();
    bool hasClipboardText = !clipboard->text().isEmpty();
    m_pasteCellAction->setEnabled(hasClipboardText);

    // Show context menu
    m_cellMenu->exec(ui->tableWidget->mapToGlobal(pos));
}

void StcTablesCreator::copyCellContent()
{
    if (m_contextCellRow == -1 || m_contextCellColumn == -1) {
        return;
    }

    // Get cell content
    QTableWidgetItem* item = ui->tableWidget->item(m_contextCellRow, m_contextCellColumn);
    // The STC form: a formatted cell is copied together with its [run]...[/run]
    QString cellContent = cellToStc(m_contextCellRow, m_contextCellColumn);

    // Copy to clipboard
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(cellContent);

    qDebug() << "Copied cell content to clipboard:" << cellContent
             << "from cell (" << m_contextCellRow << "," << m_contextCellColumn << ")";

    // Optional: Show brief feedback
    if (item)
    {
        QColor originalColor = item->background().color();
        item->setBackground(QColor(173, 216, 230)); // Light blue

        QTimer::singleShot(300, [=, this]() {
            if (auto* currentItem = ui->tableWidget->item(m_contextCellRow, m_contextCellColumn))
            {
                currentItem->setBackground(originalColor.isValid() ? originalColor : QColor());
            }
        });
    }
}

void StcTablesCreator::pasteCellContent()
{
    if (m_contextCellRow == -1 || m_contextCellColumn == -1) {
        return;
    }

    // Get clipboard content
    QClipboard* clipboard = QApplication::clipboard();
    QString clipboardContent = clipboard->text();

    if (clipboardContent.isEmpty()) {
        QMessageBox::information(this, tr("Informacja"),
                                tr("Schowek jest pusty lub nie zawiera tekstu."));
        return;
    }

    // Get current cell content
    QString currentContent = cellToStc(m_contextCellRow, m_contextCellColumn).trimmed();

    // If cell is not empty, show confirmation dialog
    if (!currentContent.isEmpty()) {
        showPasteConfirmationDialog(currentContent, clipboardContent);
    } else {
        // Cell is empty, paste directly
        performPaste(clipboardContent);
    }
}

void StcTablesCreator::showPasteConfirmationDialog(const QString& currentContent, const QString& clipboardContent)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Potwierdź wklejenie"));
    msgBox.setIcon(QMessageBox::Question);

    // Create detailed message
    QString message = tr("Komórka w pozycji (%1, %2) nie jest pusta.\n\n")
                      .arg(m_contextCellRow + 1).arg(m_contextCellColumn + 1);

    message += tr("Aktualna zawartość:\n\"%1\"\n\n").arg(currentContent);
    message += tr("Zawartość schowka:\n\"%1\"\n\n").arg(clipboardContent);
    message += tr("Co chcesz zrobić?");

    msgBox.setText(message);

    // Create custom buttons
    QPushButton* replaceButton = msgBox.addButton(tr("Zastąp"), QMessageBox::AcceptRole);
    QPushButton* appendButton = msgBox.addButton(tr("Dołącz na końcu"), QMessageBox::AcceptRole);
    QPushButton* prependButton = msgBox.addButton(tr("Dołącz na początku"), QMessageBox::AcceptRole);
    QPushButton* cancelButton = msgBox.addButton(tr("Anuluj"), QMessageBox::RejectRole);

    msgBox.setDefaultButton(replaceButton);
    msgBox.setEscapeButton(cancelButton);

    msgBox.exec();

    QAbstractButton* clickedButton = msgBox.clickedButton();

    // Join the texts without their [run] wrappers; the result is formatted when any of the parts is
    const auto current = stc::tablecell::decode(currentContent);
    const auto clipboard = stc::tablecell::decode(clipboardContent);
    const bool formatted = current.formatted || clipboard.formatted;

    if (clickedButton == replaceButton) {
        performPaste(clipboardContent);
    } else if (clickedButton == appendButton) {
        performPaste(stc::tablecell::encode(current.inner + clipboard.inner, formatted));
    } else if (clickedButton == prependButton) {
        performPaste(stc::tablecell::encode(clipboard.inner + current.inner, formatted));
    }
    // If cancel or closed, do nothing
}

void StcTablesCreator::performPaste(const QString& content)
{
    // `content` is in the STC form: [run]...[/run] around the whole text makes the cell formatted
    setCellFromStc(m_contextCellRow, m_contextCellColumn, content);
    QTableWidgetItem* item = ui->tableWidget->item(m_contextCellRow, m_contextCellColumn);
    if (!item)
    {
        return;
    }

    // Visual feedback
    QColor originalColor = item->background().color();
    item->setBackground(QColor(144, 238, 144)); // Light green

    QTimer::singleShot(500, [=, this]() {
        if (auto* currentItem = ui->tableWidget->item(m_contextCellRow, m_contextCellColumn))
        {
            currentItem->setBackground(originalColor.isValid() ? originalColor : QColor());
        }
    });

    qDebug() << "Pasted content to cell (" << m_contextCellRow << "," << m_contextCellColumn << "):"
             << content;
}

StcTablesCreator::~StcTablesCreator()
{
    delete ui;
    delete m_rowMenu;
    delete m_columnMenu;
}

void StcTablesCreator::setHeaderEnabled(bool enabled)
{
    m_hasHeader = enabled;
    if (ui && ui->headerCheckBox)
    {
        ui->headerCheckBox->setChecked(enabled);
    }
    applyHeaderStyle();
}

void StcTablesCreator::setExtendedEnabled(bool enabled)
{
    m_isExtended = enabled;
    if (ui && ui->extendedCheckBox)
    {
        ui->extendedCheckBox->setChecked(enabled);
    }
}

void StcTablesCreator::onAddColumnRight()
{
    int colCount = ui->tableWidget->columnCount();
    ui->tableWidget->insertColumn(colCount);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(colCount, QHeaderView::Stretch);
}

void StcTablesCreator::onAddRowBelow()
{
    insertRowAt(ui->tableWidget->rowCount());
}

void StcTablesCreator::insertRowAt(int row)
{
    ui->tableWidget->insertRow(row);
    // Select the new row
    ui->tableWidget->selectRow(row);
}

void StcTablesCreator::showRowContextMenu(const QPoint &pos)
{
    QTableWidgetItem *item = ui->tableWidget->itemAt(pos);
    if (!item)
        return;

    m_contextMenuRow = item->row();
    m_contextMenuColumn = item->column();

    // Show the context menu at the cursor position
    m_rowMenu->exec(ui->tableWidget->viewport()->mapToGlobal(pos));
}

void StcTablesCreator::showRowHeaderContextMenu(const QPoint &pos)
{
    // Get the row under the cursor
    int row = ui->tableWidget->verticalHeader()->logicalIndexAt(pos);
    if (row < 0)
        return;

    m_contextMenuRow = row;
    m_contextMenuColumn = -1;

    // Show the context menu at the cursor position
    m_rowMenu->exec(ui->tableWidget->verticalHeader()->viewport()->mapToGlobal(pos));
}

void StcTablesCreator::showColumnContextMenu(const QPoint &pos)
{
    int column = ui->tableWidget->horizontalHeader()->logicalIndexAt(pos);
    if (column < 0)
        return;

    m_contextMenuColumn = column;

    // Show the context menu at the cursor position
    m_columnMenu->exec(ui->tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos));
}

void StcTablesCreator::deleteSelectedRows()
{
    if (m_contextMenuRow < 0)
        return;

    // Remove the selected rows
    ui->tableWidget->removeRow(m_contextMenuRow);

    // Reset context menu position
    m_contextMenuRow = -1;
}

void StcTablesCreator::insertRowAbove()
{
    if (m_contextMenuRow < 0)
        return;

    // Insert a new row above the selected row
    ui->tableWidget->insertRow(m_contextMenuRow);

    // Select the new row
    ui->tableWidget->selectRow(m_contextMenuRow);

    // Reset context menu position
    m_contextMenuRow = -1;
}

void StcTablesCreator::insertRowBelow()
{
    if (m_contextMenuRow < 0)
        return;

    // Insert a new row below the selected row
    int newRow = m_contextMenuRow + 1;
    ui->tableWidget->insertRow(newRow);

    // Select the new row
    ui->tableWidget->selectRow(newRow);

    // Reset context menu position
    m_contextMenuRow = -1;
}

void StcTablesCreator::deleteSelectedColumns()
{
    if (m_contextMenuColumn < 0)
        return;

    // Remove the selected column
    ui->tableWidget->removeColumn(m_contextMenuColumn);

    // Reset context menu position
    m_contextMenuColumn = -1;
}

void StcTablesCreator::insertColumnLeft()
{
    if (m_contextMenuColumn < 0)
        return;

    // Insert a new column to the left of the selected column
    ui->tableWidget->insertColumn(m_contextMenuColumn);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(m_contextMenuColumn, QHeaderView::Stretch);

    // Select the new column
    ui->tableWidget->selectColumn(m_contextMenuColumn);

    // Reset context menu position
    m_contextMenuColumn = -1;
}

void StcTablesCreator::insertColumnRight()
{
    if (m_contextMenuColumn < 0)
        return;

    // Insert a new column to the right of the selected column
    int newCol = m_contextMenuColumn + 1;
    ui->tableWidget->insertColumn(newCol);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(newCol, QHeaderView::Stretch);

    // Select the new column
    ui->tableWidget->selectColumn(newCol);

    // Reset context menu position
    m_contextMenuColumn = -1;
}

void StcTablesCreator::setupTable(const QString& content)
{
    clearTableWidget();

    if (content.trimmed().isEmpty()) {
        qDebug() << "Empty content, nothing to display";
        return;
    }

    // Parse CSV attributes and normalize content
    parseCsvAttributes(content);
    QString normalized = normalizeLineEndings(content);
    QStringList rawLines = splitContentIntoLines(normalized);
    QStringList processedLines = processAndTrimLines(rawLines);

    if (processedLines.isEmpty())
    {
        qDebug() << "No valid lines found after processing";
        return;
    }

    // Analyze content structure (the header is simply the first row; `header` attribute says it is one)
    int maxColumns = calculateMaxColumnCount(processedLines);

    if (maxColumns == 0) {
        qDebug() << "No columns detected";
        return;
    }

    // Configure and populate table
    configureTableDimensions(maxColumns, processedLines.size());
    populateTableWithData(processedLines, maxColumns);
    finalizeTableAppearance();
}

void StcTablesCreator::clearTableWidget()
{
    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setColumnCount(0);
}

void StcTablesCreator::parseCsvAttributes(const QString& content)
{
    static QRegularExpression csvTagRe(R"(\[csv(?:\s+([^\]]+))?\])");
    QRegularExpressionMatch match = csvTagRe.match(content);

    if (match.hasMatch()) {
        QString attributes = match.captured(1);
        m_hasHeader = attributes.contains("header");
        m_isExtended = attributes.contains("extended");

        // Update UI checkboxes to reflect parsed attributes
        ui->headerCheckBox->setChecked(m_hasHeader);
        ui->extendedCheckBox->setChecked(m_isExtended);
    }
}

QString StcTablesCreator::normalizeLineEndings(const QString& content)
{
    QString normalized = content;
    // Replace all possible line endings with Unix style
    normalized.replace("\r\n", "\n")  // Windows
               .replace('\r', '\n')       // Old Mac
               .replace(QChar(0x2028), '\n')  // Line Separator
               .replace(QChar(0x2029), '\n'); // Paragraph Separator
    return normalized;
}

QStringList StcTablesCreator::splitContentIntoLines(const QString& normalizedContent)
{
    QStringList lines;
    QString currentLine;
    bool inRunBlock = false;

    for (int i = 0; i < normalizedContent.length(); ++i) {
        QChar c = normalizedContent[i];

        // Check for [run] or [/run] tags to preserve content within run blocks
        if (normalizedContent.mid(i, 5) == "[run]") {
            inRunBlock = true;
            currentLine += "[run]";
            i += 4; // Skip the rest of the tag
            continue;
        }
        else if (normalizedContent.mid(i, 6) == "[/run]") {
            inRunBlock = false;
            currentLine += "[/run]";
            i += 5; // Skip the rest of the tag
            continue;
        }

        // Handle newline only when not in a run block
        if (!inRunBlock && c == '\n') {
            lines.append(currentLine);
            currentLine.clear();
        }
        else {
            currentLine += c;
        }
    }

    // Add the last line if not empty or if we have no lines at all
    if (!currentLine.isEmpty() || lines.isEmpty()) {
        lines.append(currentLine);
    }

    return lines;
}

QStringList StcTablesCreator::processAndTrimLines(const QStringList& rawLines)
{
    QStringList processedLines = rawLines;

    // Trim all lines
    for (int i = 0; i < processedLines.size(); ++i) {
        processedLines[i] = processedLines[i].trimmed();
    }

    // Remove any empty lines at the end
    while (!processedLines.isEmpty() && processedLines.last().isEmpty()) {
        processedLines.removeLast();
    }

    return processedLines;
}

int StcTablesCreator::calculateMaxColumnCount(const QStringList& lines)
{
    int maxColumns = 0;

    for (const QString& line : lines) {
        int columns = countColumnsInLine(line);
        if (columns > maxColumns) {
            maxColumns = columns;
        }
    }

    qDebug() << "Detected columns:" << maxColumns;
    return maxColumns;
}

int StcTablesCreator::countColumnsInLine(const QString& line)
{
    int columns = 1; // At least one column
    bool inQuotes = false;
    bool inTag = false;

    for (int i = 0; i < line.length(); ++i) {
        QChar c = line[i];

        if (c == '[') {
            inTag = true;
        }
        else if (c == ']') {
            inTag = false;
        }
        else if (c == '"') {
            inQuotes = !inQuotes;
        }
        else if (c == ';' && !inQuotes && !inTag) {
            columns++;
        }
    }

    return columns;
}

void StcTablesCreator::configureTableDimensions(int maxColumns, int rowCount)
{
    ui->tableWidget->setColumnCount(maxColumns);
    ui->tableWidget->setRowCount(rowCount);
}

void StcTablesCreator::populateTableWithData(const QStringList& dataLines, int maxColumns)
{
    for (int row = 0; row < dataLines.size(); ++row)
    {
        const QString& line = dataLines[row];
        QStringList cells = parseLineIntoCells(line);

        // Fill the row with parsed cell data (every cell gets an item - the check box shows if it is formatted)
        for (int col = 0; col < maxColumns; ++col)
        {
            setCellFromStc(row, col, col < cells.size() ? cells[col] : QString());
        }
    }
}

QStringList StcTablesCreator::parseLineIntoCells(const QString& line)
{
    QStringList cells;
    QString currentCell;
    bool inQuotes = false;
    bool inTag = false;

    for (int i = 0; i < line.length(); ++i)
    {
        QChar c = line[i];

        // Handle tag start/end
        if (c == '[')
        {
            inTag = true;
            currentCell += c;
        }
        else if (c == ']')
        {
            inTag = false;
            currentCell += c;
        }
        // Handle quoted text
        else if (c == '"')
        {
            inQuotes = !inQuotes;
            currentCell += c;
        }
        // Handle cell separator (only if not in quotes and not in a tag)
        else if (c == ';' && !inQuotes && !inTag)
        {
            cells.append(currentCell.trimmed());
            currentCell.clear();
        }
        // Regular character
        else
        {
            currentCell += c;
        }
    }

    // Add the last cell
    if (!currentCell.isEmpty() || !cells.isEmpty())
    {
        cells.append(currentCell.trimmed());
    }

    return cells;
}

void StcTablesCreator::finalizeTableAppearance()
{
    fillMissingItems();
    applyHeaderStyle();

    // Resize columns to fit content optimally
    ui->tableWidget->resizeColumnsToContents();
}

QString StcTablesCreator::generateTableContent() const
{
    return m_generatedContent;
}

QString StcTablesCreator::generateTableContentImpl() const
{
    // `[run]` works only in the extended tables
    QStringList attributes;
    if (m_hasHeader) attributes << "header";
    if (m_isExtended || hasFormattedCells()) attributes << "extended";

    QString result = "[csv";
    if (!attributes.isEmpty())
    {
        result += " " + attributes.join(" ");
    }
    result += "]\n";

    // The header is the first row of the table, so every row is written the same way
    QStringList lines;
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        QStringList rowData;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            rowData.append(cellToStc(row, col));
        }
        lines.append(rowData.join(";"));
    }
    result += lines.join("\n");

    result += "\n[/csv]";
    return result;
}

void StcTablesCreator::accept()
{
    // An edit which is still open (OK pressed from the keyboard) must not be lost
    if (m_activeEditor)
    {
        emit m_cellDelegate->commitData(m_activeEditor);
        emit m_cellDelegate->closeEditor(m_activeEditor, QAbstractItemDelegate::NoHint);
    }

    // Store the generated content before accepting
    m_generatedContent = generateTableContentImpl();

    // Call the base class accept() to close the dialog
    QDialog::accept();
}

// ------------------------------------------------------------------------------------------------
// Formatted ([run]) cells
// ------------------------------------------------------------------------------------------------

void StcTablesCreator::setupCellEditing()
{
    QTableWidget* table = ui->tableWidget;

    // Items created by the view itself (editing an empty cell) get the check box as well
    table->setItemPrototype(new StcCellItem);

    m_cellDelegate = new StcCellDelegate(this);
    table->setItemDelegate(m_cellDelegate);

    // first line: is the current cell formatted ([run])?
    m_formattedButton = new QToolButton(this);
    m_formattedButton->setText(tr("[run] formatted cell"));
    m_formattedButton->setToolTip(tr("Formatted cell: the text is wrapped in [run]...[/run] and the STC tags work in it.\n"
                                     "Not formatted cell: plain text, tags are shown as they are written.\n"
                                     "The same as the check box in the cell."));
    m_formattedButton->setCheckable(true);
    m_formattedButton->setFocusPolicy(Qt::NoFocus); // the cell editor has to keep the focus
    ui->gridLayout->addWidget(m_formattedButton, 0, 0, 1, 2);

    auto* hint = new QLabel(tr("Enter - accept, Shift+Enter - new line"), this);
    ui->gridLayout->addWidget(hint, 0, 2);

    // on the right: the same buttons of the tags as in the main window (without [csv] and [run])
    m_tagsButtons = new StcTagsButtons(this);
    m_tagsButtons->useInTableCell();
    m_tagsButtons->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    ui->gridLayout->addWidget(m_tagsButtons, 0, 5, 4, 1, Qt::AlignTop);

    connect(m_cellDelegate, &StcCellDelegate::cellEditorCreated, this,
            [this](QPlainTextEdit* editor, bool) { onCellEditorCreated(editor); });
    connect(m_tagsButtons, &StcTagsButtons::buttonPressed, this, &StcTablesCreator::onPaletteTagRequested);
    connect(m_formattedButton, &QToolButton::clicked, this, &StcTablesCreator::onFormattedToggled);
    connect(table, &QTableWidget::currentCellChanged, this, [this] { onCurrentCellChanged(); });

    // New rows / columns (the buttons and the menus) get the cells with the check box
    auto refill = [this] {
        fillMissingItems();
        applyHeaderStyle();
    };
    connect(table->model(), &QAbstractItemModel::rowsInserted, this, refill);
    connect(table->model(), &QAbstractItemModel::columnsInserted, this, refill);
    connect(table->model(), &QAbstractItemModel::rowsRemoved, this, [this] { applyHeaderStyle(); });

    fillMissingItems();
    applyHeaderStyle();
    updatePaletteState();
}

void StcTablesCreator::fillMissingItems()
{
    QSignalBlocker blocker(ui->tableWidget);
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            if (!ui->tableWidget->item(row, col))
            {
                ui->tableWidget->setItem(row, col, new StcCellItem);
            }
        }
    }
}

void StcTablesCreator::applyHeaderStyle()
{
    if (!ui)
    {
        return;
    }
    QSignalBlocker blocker(ui->tableWidget);
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        // with the header the first row is called "header" and the numbering starts from the next one
        const QString label = m_hasHeader ? (row == 0 ? QStringLiteral("header") : QString::number(row)) : QString::number(row + 1);
        QTableWidgetItem* rowLabel = ui->tableWidget->verticalHeaderItem(row);
        if (!rowLabel)
        {
            rowLabel = new QTableWidgetItem;
            ui->tableWidget->setVerticalHeaderItem(row, rowLabel);
        }
        rowLabel->setText(label);
        QFont labelFont = ui->tableWidget->verticalHeader()->font();
        labelFont.setBold(m_hasHeader && row == 0);
        rowLabel->setFont(labelFont);

        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            if (QTableWidgetItem* item = ui->tableWidget->item(row, col))
            {
                QFont font = ui->tableWidget->font();
                font.setBold(m_hasHeader && row == 0);
                item->setFont(font);
            }
        }
    }
}

void StcTablesCreator::refreshItemDiagnostics(QTableWidgetItem* item)
{
    const bool formatted = item->checkState() == Qt::Checked;

    QStringList problems;
    if (stc::tablecell::hasRunInsideUnwrapped(item->text(), formatted))
    {
        problems << tr("There is [run] inside of the text. Tick the box to make the whole cell formatted instead.");
    }
    if (stc::tablecell::separatorsInText(item->text()) > 0)
    {
        problems << tr("There is ';' outside of a tag - it would split this cell in two.");
    }

    QSignalBlocker blocker(ui->tableWidget);
    if (problems.isEmpty())
    {
        item->setData(Qt::ToolTipRole, QVariant());
        item->setData(Qt::DecorationRole, QVariant());
        item->setData(Qt::ForegroundRole, QVariant());
    }
    else
    {
        item->setData(Qt::ToolTipRole, problems.join("\n"));
        item->setData(Qt::DecorationRole, QIcon::fromTheme("dialog-warning"));
        item->setData(Qt::ForegroundRole, QBrush(QColor(200, 60, 0)));
    }
}

void StcTablesCreator::setCellFromStc(int row, int column, const QString& stc)
{
    const auto decoded = stc::tablecell::decode(stc);
    // plain cells are one line (a line break would start a new row)
    const QString inner = decoded.formatted ? decoded.inner : stc::tablecell::encode(decoded.inner, false);

    QSignalBlocker blocker(ui->tableWidget);
    QTableWidgetItem* item = ui->tableWidget->item(row, column);
    if (!item)
    {
        item = new StcCellItem;
        ui->tableWidget->setItem(row, column, item);
    }
    item->setText(inner);
    item->setCheckState(decoded.formatted ? Qt::Checked : Qt::Unchecked);
    refreshItemDiagnostics(item);
}

QString StcTablesCreator::cellToStc(int row, int column) const
{
    const QTableWidgetItem* item = ui->tableWidget->item(row, column);
    if (!item)
    {
        return {};
    }
    return stc::tablecell::encode(item->text(), item->checkState() == Qt::Checked);
}

bool StcTablesCreator::hasFormattedCells() const
{
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            const QTableWidgetItem* item = ui->tableWidget->item(row, col);
            if (item && item->checkState() == Qt::Checked && !item->text().isEmpty())
            {
                return true;
            }
        }
    }
    return false;
}

void StcTablesCreator::onCellEditorCreated(QPlainTextEdit* editor)
{
    m_activeEditor = editor;
    connect(editor, &QObject::destroyed, this, [this] { updatePaletteState(); });
    updatePaletteState();
}

void StcTablesCreator::onCurrentCellChanged()
{
    updatePaletteState();
}

void StcTablesCreator::updatePaletteState()
{
    if (!m_tagsButtons)
    {
        return;
    }
    const QTableWidgetItem* item = ui->tableWidget->currentItem();
    const bool formatted = item && item->checkState() == Qt::Checked;
    m_formattedButton->setEnabled(item != nullptr);
    {
        QSignalBlocker blocker(m_formattedButton);
        m_formattedButton->setChecked(formatted);
    }
    m_tagsButtons->setEnabled(formatted); // tags are interpreted only in the formatted cells
}

void StcTablesCreator::onPaletteTagRequested(StcTags tag)
{
    QTableWidgetItem* item = ui->tableWidget->currentItem();
    if (!item || item->checkState() != Qt::Checked)
    {
        return;
    }

    bool openedNow = false;
    if (!m_activeEditor)
    {
        // no editor yet: open it, the tag then goes around the whole text of the cell
        ui->tableWidget->editItem(item);
        openedNow = true;
    }
    if (!m_activeEditor)
    {
        return;
    }
    if (openedNow)
    {
        m_activeEditor->selectAll();
    }
    stcApplyTag(m_activeEditor, tag);
    m_activeEditor->setFocus();
}

void StcTablesCreator::onFormattedToggled(bool formatted)
{
    QTableWidgetItem* item = ui->tableWidget->currentItem();
    if (!item)
    {
        return;
    }
    // finish the edit first, so that what was typed is not lost and the editor gets rebuilt next time
    if (m_activeEditor)
    {
        emit m_cellDelegate->commitData(m_activeEditor);
        emit m_cellDelegate->closeEditor(m_activeEditor, QAbstractItemDelegate::NoHint);
    }
    if (!formatted)
    {
        item->setText(stc::tablecell::encode(item->text(), false)); // plain cell = one line
    }
    item->setCheckState(formatted ? Qt::Checked : Qt::Unchecked); // itemChanged refreshes the rest
}
