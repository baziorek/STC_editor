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
    QStringList lines = content.split('\n', Qt::SkipEmptyParts);
    // if (lines.isEmpty())
    //     return;
    
    // Determine if we have a header row
    bool hasHeader = false;
    if (content.contains("\n"))
    {
        // Check if the first line is a header (non-empty and doesn't start with ';')
        QString firstLine = lines.first().trimmed();
        hasHeader = !firstLine.isEmpty() && !firstLine.startsWith(';');
    }
    
    // Clear existing content
    ui->tableWidget->clear();
    
    // Set up rows and columns
    int rowCount = lines.size() - (hasHeader ? 1 : 0);
    int colCount = 0;
    
    // Find maximum number of columns
    for (const QString& line : lines)
    {
        int cols = line.count(';') + 1;
        if (cols > colCount) {
            colCount = cols;
        }
    }
    
    ui->tableWidget->setRowCount(rowCount);
    ui->tableWidget->setColumnCount(colCount);
    
    // Fill the table
    int row = 0;
    for (int i = 0; i < lines.size(); ++i)
    {
        if (i == 0 && hasHeader)
        {
            // Skip header for now, we'll handle it after setting up the table
            continue;
        }
        
        QStringList cells = lines[i].split(';');
        for (int col = 0; col < cells.size() && col < colCount; ++col) {
            QString cell = cells[col].trimmed();
            // Remove any STC formatting tags if present
            cell.remove(QRegularExpression("\\[.*\\]"));
            QTableWidgetItem *item = new QTableWidgetItem(cell);
            ui->tableWidget->setItem(row, col, item);
        }
        
        if (i > 0 || !hasHeader) {
            row++;
        }
    }
    
    // Set header if present
    if (hasHeader)
    {
        QStringList headers = lines.first().split(';');
        for (int col = 0; col < headers.size() && col < colCount; ++col) {
            QString header = headers[col].trimmed();
            // Remove any STC formatting tags if present
            header.remove(QRegularExpression("\\[.*\\]"));
            ui->tableWidget->setHorizontalHeaderItem(col, new QTableWidgetItem(header));
        }
        m_hasHeader = true;
    }
    
    // Resize columns to fit content
    ui->tableWidget->resizeColumnsToContents();
}

QString StcTablesCreator::generateTableContent() const
{
    QStringList lines;
    
    // Add header if present
    if (m_hasHeader)
    {
        QStringList headers;
        for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
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
