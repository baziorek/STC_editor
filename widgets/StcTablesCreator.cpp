#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
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
{
    ui->setupUi(this);
    setWindowTitle(tr("Table Editor"));
    
    // Set up the table
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    // Connect buttons
    connect(ui->addColumnButton, &QPushButton::clicked, this, &StcTablesCreator::onAddColumnRight);
    connect(ui->addRowButton, &QPushButton::clicked, this, &StcTablesCreator::onAddRowBelow);
    
    // Add OK/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &StcTablesCreator::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout)
    {
        mainLayout->addWidget(buttonBox);
    }
    
    // Parse and set up the table content if provided
    if (! tableContent.isEmpty())
    {
        setupTable(tableContent);
    }
}

StcTablesCreator::~StcTablesCreator()
{
    delete ui;
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
    int rowCount = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(rowCount);
}

void StcTablesCreator::onAccepted()
{
    // Any additional validation can be added here
    accept();
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
    qDebug() << "Table setup complete";
}

QString StcTablesCreator::generateTableContent() const
{
    QStringList lines;
    
    // Add header if present
    if (m_hasHeader)
    {
        QStringList headers;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *header = ui->tableWidget->horizontalHeaderItem(col);
            headers.append(header ? header->text() : QString());
        }
        lines.append(headers.join(';'));
    }
    
    // Add data rows
    for (int row = 0; row < ui->tableWidget->rowCount(); ++row)
    {
        QStringList cells;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col)
        {
            QTableWidgetItem *item = ui->tableWidget->item(row, col);
            cells.append(item ? item->text() : QString());
        }
        lines.append(cells.join(';'));
    }
    
    return lines.join('\n');
}
