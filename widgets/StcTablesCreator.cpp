#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QCheckBox>
#include <QDebug>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QRegularExpression>
#include <QBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include "StcTablesCreator.h"
#include "ui_StcTablesCreator.h"


StcTablesCreator::StcTablesCreator(const QString& tableContent, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StcTablesCreator)
    , m_hasHeader(false)
    , m_isExtended(false)
    , m_rowMenu(nullptr)
    , m_columnMenu(nullptr)
    , m_deleteRowAction(nullptr)
    , m_insertRowAboveAction(nullptr)
    , m_insertRowBelowAction(nullptr)
    , m_deleteColumnAction(nullptr)
    , m_insertColumnLeftAction(nullptr)
    , m_insertColumnRightAction(nullptr)
    , m_contextMenuRow(-1)
    , m_contextMenuColumn(-1)
{
    ui->setupUi(this);
    setWindowTitle(tr("Table Editor"));
    
    // Set up the table with default size and stretch columns
    ui->tableWidget->setColumnCount(2);
    ui->tableWidget->setRowCount(2);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    // Enable context menu for the table and vertical header
    ui->tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget, &QTableWidget::customContextMenuRequested, 
            this, &StcTablesCreator::showRowContextMenu);
    
    // Enable context menu for the horizontal header
    ui->tableWidget->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &StcTablesCreator::showColumnContextMenu);
            
    // Enable context menu for the vertical header
    ui->tableWidget->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableWidget->verticalHeader(), &QHeaderView::customContextMenuRequested,
            this, &StcTablesCreator::showRowHeaderContextMenu);
    
    // Create row context menu and actions
    m_rowMenu = new QMenu(this);
    m_deleteRowAction = new QAction(tr("Delete Row"), this);
    m_insertRowAboveAction = new QAction(tr("Insert Row Above"), this);
    m_insertRowBelowAction = new QAction(tr("Insert Row Below"), this);
    
    connect(m_deleteRowAction, &QAction::triggered, this, &StcTablesCreator::deleteSelectedRows);
    connect(m_insertRowAboveAction, &QAction::triggered, this, &StcTablesCreator::insertRowAbove);
    connect(m_insertRowBelowAction, &QAction::triggered, this, &StcTablesCreator::insertRowBelow);
    
    m_rowMenu->addAction(m_insertRowAboveAction);
    m_rowMenu->addAction(m_insertRowBelowAction);
    m_rowMenu->addSeparator();
    m_rowMenu->addAction(m_deleteRowAction);
    
    // Create column context menu and actions
    m_columnMenu = new QMenu(this);
    m_deleteColumnAction = new QAction(tr("Delete Column"), this);
    m_insertColumnLeftAction = new QAction(tr("Insert Column Left"), this);
    m_insertColumnRightAction = new QAction(tr("Insert Column Right"), this);
    
    connect(m_deleteColumnAction, &QAction::triggered, this, &StcTablesCreator::deleteSelectedColumns);
    connect(m_insertColumnLeftAction, &QAction::triggered, this, &StcTablesCreator::insertColumnLeft);
    connect(m_insertColumnRightAction, &QAction::triggered, this, &StcTablesCreator::insertColumnRight);
    
    m_columnMenu->addAction(m_insertColumnLeftAction);
    m_columnMenu->addAction(m_insertColumnRightAction);
    m_columnMenu->addSeparator();
    m_columnMenu->addAction(m_deleteColumnAction);
    
    // Initialize context menu positions
    m_contextMenuRow = -1;
    m_contextMenuColumn = -1;
    
    // Connect buttons
    connect(ui->addColumnButton, &QPushButton::clicked, this, [this]() {
        int col = ui->tableWidget->columnCount();
        ui->tableWidget->insertColumn(col);
        ui->tableWidget->horizontalHeader()->setSectionResizeMode(col, QHeaderView::Stretch);
    });
    
    connect(ui->addRowButton, &QPushButton::clicked, this, [this]() {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);
    });
    
    // Initialize checkboxes
    ui->headerCheckBox->setChecked(m_hasHeader);
    ui->extendedCheckBox->setChecked(m_isExtended);
    
    // Connect checkboxes to update flags
    connect(ui->headerCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_hasHeader = checked;
    });
    
    connect(ui->extendedCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_isExtended = checked;
    });
    
    // Set up OK/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &StcTablesCreator::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // Add buttons to layout
    ui->gridLayout->addWidget(buttonBox, 2, 0, 1, 2);
    
    // Set up the table with content if provided
    if (!tableContent.isEmpty())
    {
        setupTable(tableContent);
    }
}

StcTablesCreator::~StcTablesCreator()
{
    delete m_rowMenu;
    delete m_columnMenu;
    delete ui;
}

void StcTablesCreator::setHeaderEnabled(bool enabled)
{
    m_hasHeader = enabled;
    if (ui && ui->headerCheckBox)
    {
        ui->headerCheckBox->setChecked(enabled);
    }
}

void StcTablesCreator::setExtendedEnabled(bool enabled)
{
    m_isExtended = enabled;
    if (ui && ui->extendedCheckBox)
    {
        ui->extendedCheckBox->setChecked(enabled);
    }
}

QString StcTablesCreator::getTableContent() const
{
    return generateTableContent();
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

void StcTablesCreator::onAccepted()
{
    // Any additional validation can be added here
    accept();
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
    qDebug() << "-----------------";
    qDebug() << "Original content:" << content << "::" << content.count('\n')
             << "Line separators:" << content.count(QChar(0x2028))
             << "Paragraph separators:" << content.count(QChar(0x2029));

    // Clear the table
    ui->tableWidget->clear();
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->setColumnCount(0);

    if (content.trimmed().isEmpty())
    {
        qDebug() << "Empty content, nothing to display";
        return;
    }
    
    // Parse CSV tag attributes if present
    static QRegularExpression csvTagRe(R"(\[csv(?:\s+([^\]]+))?\])");
    QRegularExpressionMatch match = csvTagRe.match(content);
    if (match.hasMatch())
    {
        QString attributes = match.captured(1);
        m_hasHeader = attributes.contains("header");
        m_isExtended = attributes.contains("extended");
        
        // Update checkboxes
        ui->headerCheckBox->setChecked(m_hasHeader);
        ui->extendedCheckBox->setChecked(m_isExtended);
    }

    // First, normalize all possible line endings to Unix style (\n)
    QString normalized = content;
    // Replace all possible line endings with Unix style
    normalized.replace("\r\n", "\n")  // Windows
           .replace('\r', '\n')       // Old Mac
           .replace(QChar(0x2028), '\n')  // Line Separator
           .replace(QChar(0x2029), '\n'); // Paragraph Separator
    
    // Split into lines while handling [run]...[/run] blocks
    QStringList lines;
    QString currentLine;
    bool inRunBlock = false;
    
    for (int i = 0; i < normalized.length(); ++i)
    {
        QChar c = normalized[i];
        
        // Check for [run] or [/run] tags
        if (normalized.mid(i, 5) == "[run]")
        {
            inRunBlock = true;
            currentLine += "[run]";
            i += 4; // Skip the rest of the tag
            continue;
        }
        else if (normalized.mid(i, 6) == "[/run]")
        {
            inRunBlock = false;
            currentLine += "[/run]";
            i += 5; // Skip the rest of the tag
            continue;
        }
        
        // Handle newline only when not in a run block
        if (!inRunBlock && c == '\n')
        {
            lines.append(currentLine);
            currentLine.clear();
        }
        else
        {
            currentLine += c;
        }
    }
    
    // Add the last line if not empty
    if (!currentLine.isEmpty() || lines.isEmpty())
    {
        lines.append(currentLine);
    }
    
    // Trim all lines and remove any empty lines at the end
    for (int i = 0; i < lines.size(); ++i)
    {
        lines[i] = lines[i].trimmed();
    }
    while (!lines.isEmpty() && lines.last().isEmpty())
    {
        lines.removeLast();
    }

    qDebug() << "Processed lines (" << lines.size() << "):";
    for (int i = 0; i < lines.size(); ++i)
    {
        qDebug() << "[" << i << "]" << lines[i];
    }

    if (lines.isEmpty())
    {
        qDebug() << "No valid lines found after processing";
        return;
    }

    // Check for header based on the first line not starting with ';'
    bool hasHeader = !lines.isEmpty() && !lines.first().trimmed().startsWith(';');
    m_hasHeader = hasHeader;
    
    // Make a copy of all lines for processing
    QStringList dataLines = lines;

    // Determine number of columns (maximum number of cells in any row)
    int maxColumns = 0;
    for (const QString& line : lines)
    {
        int columns = 1; // At least one column
        bool inQuotes = false;
        bool inTag = false;
        
        for (int i = 0; i < line.length(); ++i)
        {
            QChar c = line[i];
            
            if (c == '[')
            {
                inTag = true;
            }
            else if (c == ']')
            {
                inTag = false;
            }
            else if (c == '"')
            {
                inQuotes = !inQuotes;
            }
            else if (c == ';' && !inQuotes && !inTag)
            {
                columns++;
            }
        }
        
        if (columns > maxColumns)
        {
            maxColumns = columns;
        }
    }

    qDebug() << "Detected columns:" << maxColumns;
    if (maxColumns == 0)
    {
        qDebug() << "No columns detected";
        return;
    }

    // Set up the table with the correct number of columns and rows
    ui->tableWidget->setColumnCount(maxColumns);
    ui->tableWidget->setRowCount(dataLines.size());

    // Set header if present
    if (hasHeader && !lines.isEmpty())
    {
        QStringList headers;
        QString currentHeader;
        bool inQuotes = false;
        bool inTag = false;
        
        const QString& headerLine = lines.first();
        for (int i = 0; i < headerLine.length(); ++i)
        {
            QChar c = headerLine[i];
            
            if (c == '[')
                inTag = true;
            else if (c == ']')
                inTag = false;
            
            if (c == ';' && !inQuotes && !inTag)
            {
                headers.append(currentHeader.trimmed());
                currentHeader.clear();
                continue;
            }
            
            if (c == '"')
                inQuotes = !inQuotes;
            currentHeader += c;
        }
        
        // Add the last header
        if (!currentHeader.isEmpty() || !headers.isEmpty())
        {
            headers.append(currentHeader.trimmed());
        }
        
        // Set header items
        for (int col = 0; col < headers.size() && col < maxColumns; ++col)
        {
            ui->tableWidget->setHorizontalHeaderItem(col, new QTableWidgetItem(headers[col]));
        }
    }

    // Parse each line and fill the table
    for (int row = 0; row < dataLines.size(); ++row)
    {
        const QString& line = dataLines[row];
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

        // Fill the row with cells
        for (int col = 0; col < cells.size() && col < maxColumns; ++col)
        {
            QTableWidgetItem* item = new QTableWidgetItem(cells[col]);
            ui->tableWidget->setItem(row, col, item);
        }
    }

    // Resize columns to fit content
    ui->tableWidget->resizeColumnsToContents();
}

QString StcTablesCreator::generateTableContent() const
{
    // Build CSV attributes
    QStringList attributes;
    if (m_hasHeader) attributes << "header";
    if (m_isExtended) attributes << "extended";

    QString result = "[csv";
    if (!attributes.isEmpty())
    {
        result += " " + attributes.join(" ");
    }
    result += "]\n";

    // Add header if present
    if (m_hasHeader)
    {
        QStringList headers;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *header = ui->tableWidget->horizontalHeaderItem(col);
            headers.append(header ? header->text() : QString());
        }
        result += headers.join(";") + "\n";
    }

    // Add table content
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        QStringList rowData;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *item = ui->tableWidget->item(row, col);
            QString cellContent = item ? item->text() : QString();
            rowData.append(cellContent);
        }
        result += rowData.join(";");
        if (row < ui->tableWidget->rowCount() - 1) {
            result += "\n";
        }
    }

    result += "\n[/csv]";
    return result;
}
