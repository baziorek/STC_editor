#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>
#include <QFont>
#include <QDateTime>
#include "BackupRecoveryDialog.h"
#include "CodeEditor.h"
#include "utils/DiffCalculation.h"
#include "utils/BackupManager.h"

BackupRecoveryDialog::BackupRecoveryDialog(CodeEditor* editor, const QString& originalFilePath, 
                                           const QString& backupContent, QWidget* parent)
    : QDialog(parent), m_originalFilePath(originalFilePath), m_backupContent(backupContent)
{
    setupDialog();
    setupFileInfoHeader(originalFilePath);
    setupDiffArea(editor, backupContent);
    setupButtons();
}

void BackupRecoveryDialog::setupDialog()
{
    setWindowTitle(tr("Backup Recovery"));
    resize(1000, 700);
    mainLayout = new QVBoxLayout(this);
}

void BackupRecoveryDialog::setupFileInfoHeader(const QString& originalFilePath)
{
    QFileInfo fi(originalFilePath);

    fileLabel = new QLabel(fi.fileName(), this);
    QFont f = font();
    f.setPointSize(f.pointSize() + 2);
    f.setBold(true);
    fileLabel->setFont(f);

    filePathLabel = new QLabel(fi.absolutePath(), this);
    QFont smallFont = font();
    smallFont.setPointSizeF(font().pointSizeF() - 1);
    filePathLabel->setFont(smallFont);
    filePathLabel->setStyleSheet("color: gray");

    timestampLabel = new QLabel(this);
    timestampLabel->setStyleSheet("color: gray");

    // Compare timestamps
    QDateTime backupTime = BackupManager::getBackupModificationTime(originalFilePath);
    QDateTime originalTime = BackupManager::getOriginalFileModificationTime(originalFilePath);
    
    QString timestampText;
    if (backupTime.isValid() && originalTime.isValid())
    {
        if (backupTime > originalTime)
        {
            timestampText = tr("Backup is newer (created: %1) | Original file (modified: %2)")
                            .arg(backupTime.toString("yyyy-MM-dd hh:mm:ss"))
                            .arg(originalTime.toString("yyyy-MM-dd hh:mm:ss"));
            timestampLabel->setStyleSheet("color: orange; font-weight: bold;");
        }
        else
        {
            timestampText = tr("Backup is older (created: %1) | Original file (modified: %2)")
                            .arg(backupTime.toString("yyyy-MM-dd hh:mm:ss"))
                            .arg(originalTime.toString("yyyy-MM-dd hh:mm:ss"));
        }
    }
    else if (backupTime.isValid())
    {
        timestampText = tr("Backup created: %1").arg(backupTime.toString("yyyy-MM-dd hh:mm:ss"));
    }
    else
    {
        timestampText = tr("Backup timestamp unavailable");
    }
    
    timestampLabel->setText(timestampText);

    auto* headerLayout = new QVBoxLayout();
    headerLayout->addWidget(fileLabel);
    headerLayout->addWidget(filePathLabel);
    headerLayout->addWidget(timestampLabel);
    mainLayout->addLayout(headerLayout);
}

void BackupRecoveryDialog::setupDiffArea(CodeEditor* editor, const QString& backupContent)
{
    diffWidget = new DiffViewerWidget(this);
    mainLayout->addWidget(diffWidget, 1); // 1 means Expanding

    const QStringList backupLines = backupContent.split('\n');
    const QStringList currentLines = editor->toPlainText().split('\n');

    const auto diffLines = DiffCalculation::computeDiff(backupLines, currentLines);
    const auto diffs = DiffCalculation::computeModifiedLineDiffs(diffLines);

    int added = 0, removed = 0, modified = 0;
    for (const auto& diff : diffs)
    {
        if (diff.oldLineIndex == -1)
            ++added;
        else if (diff.newLineIndex == -1)
            ++removed;
        else
            ++modified;
    }

    QLabel* statsLabel = new QLabel(tr("Differences: Modified: %1 | Added in current: %2 | Removed from current: %3")
                                   .arg(modified).arg(added).arg(removed), this);
    statsLabel->setStyleSheet("color: gray");
    mainLayout->addWidget(statsLabel);

    diffWidget->setDiffData(diffs);
}

void BackupRecoveryDialog::setupButtons()
{
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* restoreButton = new QPushButton(tr("Restore from Backup"), this);
    QPushButton* discardButton = new QPushButton(tr("Discard Backup"), this);
    QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

    connect(restoreButton, &QPushButton::clicked, this, [this]() {
        selectedResult = RestoreBackup;
        accept();
    });
    connect(discardButton, &QPushButton::clicked, this, [this]() {
        selectedResult = DiscardBackup;
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Cancel;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addWidget(discardButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

BackupRecoveryDialog::Result BackupRecoveryDialog::userChoice() const
{
    return selectedResult;
}