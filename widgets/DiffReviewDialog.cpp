#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileInfo>
#include <QFont>
#include <QDateTime>
#include "DiffReviewDialog.h"
#include "utils/diffcalculation.h"


DiffReviewDialog::DiffReviewDialog(CodeEditor* editor, const QString &dialogTitle, const QString &dialogMessage, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(dialogTitle.isEmpty()
                   ? tr("Unsaved Changes Detected")
                   : dialogTitle);

    resize(1000, 700);
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    if (!dialogMessage.isEmpty())
    {
        QLabel* msgLabel = new QLabel(dialogMessage, this);
        msgLabel->setWordWrap(true);
        QFont font = msgLabel->font();
        font.setPointSizeF(font.pointSizeF() + 1);
        msgLabel->setFont(font);
        mainLayout->addWidget(msgLabel);
    }

    const QString filePath = editor->getFileName();
    if (filePath.isEmpty())
    {
        // File not saved - no diff
        QLabel* infoLabel = new QLabel(tr("This file has not been saved yet — no diff available."), this);
        QFont font = infoLabel->font();
        font.setItalic(true);
        font.setPointSizeF(font.pointSizeF() + 1);
        infoLabel->setFont(font);
        infoLabel->setStyleSheet("color: gray");
        infoLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(infoLabel, 1);
    }
    else
    {
        // 2. Metadata + diff
        const QFileInfo fi(filePath);

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

        modifiedStatsLabel = new QLabel(this);
        timestampLabel = new QLabel(this);
        timestampLabel->setStyleSheet("color: gray");

        auto* headerLayout = new QVBoxLayout();
        headerLayout->addWidget(fileLabel);
        headerLayout->addWidget(filePathLabel);
        headerLayout->addWidget(modifiedStatsLabel);
        headerLayout->addWidget(timestampLabel);
        mainLayout->addLayout(headerLayout);

        // Diff area
        diffWidget = new DiffViewerWidget(this);
        mainLayout->addWidget(diffWidget, 1); // Expanding

        const QStringList oldLines = editor->getOriginalLines();
        const QStringList newLines = editor->toPlainText().split('\n');

        const auto diffLines = DiffCalculation::computeDiff(oldLines, newLines);
        const auto diffs = computeModifiedLineDiffs(diffLines);

        populateMetadata(oldLines, newLines, filePath,
                         editor->getFileModificationTime(),
                         editor->getLastChangeTime(),
                         diffs);
        diffWidget->setDiffData(diffs);

        // Direct connections
        connect(diffWidget, &DiffViewerWidget::jumpToLineInEditor,
                editor, &CodeEditor::go2LineRequested);
        connect(diffWidget, &DiffViewerWidget::lineRestored, editor, [editor](int newLineIndex, const QString& restoredText) {
            QTextCursor cursor = editor->cursor4Line(newLineIndex);
            cursor.select(QTextCursor::LineUnderCursor);
            cursor.insertText(restoredText);
        });
    }

    // 3. Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* saveButton = new QPushButton(tr("Save"), this);
    QPushButton* discardButton = new QPushButton(tr("Discard Changes"), this);
    QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

    connect(saveButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Save;
        accept();
    });
    connect(discardButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Discard;
        accept();
    });
    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        selectedResult = Cancel;
        reject();
    });

    buttonLayout->addStretch();
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(discardButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

DiffReviewDialog::Result DiffReviewDialog::userChoice() const
{
    return selectedResult;
}

void DiffReviewDialog::populateMetadata(const QStringList& oldLines, const QStringList& newLines,
                                        const QString& filePath,
                                        const QDateTime& fileTime,
                                        const QDateTime& lastEditTime,
                                        const QList<DiffCalculation::LineDiffResult> &diffs)
{
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

    modifiedStatsLabel->setText(tr("Modified lines: %1 | Added: %2 | Removed: %3")
                                    .arg(modified)
                                    .arg(added)
                                    .arg(removed));

    QString timestampText = tr("File last modified: %1 | Last edit in session: %2")
                                .arg(fileTime.toString("yyyy-MM-dd hh:mm:ss"))
                                .arg(lastEditTime.toString("yyyy-MM-dd hh:mm:ss"));

    timestampLabel->setText(timestampText);
}
