#include "ThreeWayDiffDialog.h"

#include "ui_ThreeWayDiffDialog.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextStream>

#include "CodeEditor.h"
#include "ThreeWayDiffWidget.h"
#include "utils/DiffCalculator.h"
#include "utils/FileEncodingHandler.h"


ThreeWayDiffDialog::ThreeWayDiffDialog(CodeEditor *editor, QWidget *parent)
    : QDialog(parent), editor(editor), ui(new Ui::ThreeWayDiffDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Compare three files"));
    resize(1200, 750);

    baseEdit = ui->baseEdit;
    oursEdit = ui->oursEdit;
    theirsEdit = ui->theirsEdit;
    useEditorCheck = ui->useEditorCheck;
    compareButton = ui->compareButton;
    saveMergedButton = ui->saveMergedButton;
    statsLabel = ui->statsLabel;
    diffWidget = ui->diffWidget;

    diffWidget->setTitles(tr("Base"), tr("Ours"), tr("Theirs"));

    connect(ui->baseBrowseButton, &QPushButton::clicked, this, &ThreeWayDiffDialog::browseBase);
    connect(ui->oursBrowseButton, &QPushButton::clicked, this, &ThreeWayDiffDialog::browseOurs);
    connect(ui->theirsBrowseButton, &QPushButton::clicked, this, &ThreeWayDiffDialog::browseTheirs);
    connect(compareButton, &QPushButton::clicked, this, &ThreeWayDiffDialog::compare);
    connect(saveMergedButton, &QPushButton::clicked, this, &ThreeWayDiffDialog::saveMerged);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(useEditorCheck, &QCheckBox::toggled, this, [this](bool checked) {
        oursEdit->setEnabled(!checked);
        ui->oursBrowseButton->setEnabled(!checked);
        updateButtons();
    });
    connect(baseEdit, &QLineEdit::textChanged, this, &ThreeWayDiffDialog::updateButtons);
    connect(oursEdit, &QLineEdit::textChanged, this, &ThreeWayDiffDialog::updateButtons);
    connect(theirsEdit, &QLineEdit::textChanged, this, &ThreeWayDiffDialog::updateButtons);

    // prefill "ours" with the file of the current tab, if it has one
    if (editor && !editor->getFileName().isEmpty())
        oursEdit->setText(editor->getFileName());

    updateButtons();
}

ThreeWayDiffDialog::~ThreeWayDiffDialog()
{
    delete ui;
}

QStringList ThreeWayDiffDialog::linesOf(const QString &filePath) const
{
    return FileEncodingHandler::readFile(filePath).split('\n');
}

void ThreeWayDiffDialog::updateButtons()
{
    const bool oursReady = useEditorCheck->isChecked() || !oursEdit->text().trimmed().isEmpty();
    compareButton->setEnabled(!baseEdit->text().trimmed().isEmpty() && oursReady &&
                              !theirsEdit->text().trimmed().isEmpty());
}

void ThreeWayDiffDialog::browseBase()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose the base file"));
    if (!path.isEmpty())
        baseEdit->setText(path);
}

void ThreeWayDiffDialog::browseOurs()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose the \"ours\" file"));
    if (!path.isEmpty())
        oursEdit->setText(path);
}

void ThreeWayDiffDialog::browseTheirs()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Choose the \"theirs\" file"));
    if (!path.isEmpty())
        theirsEdit->setText(path);
}

void ThreeWayDiffDialog::compare()
{
    QStringList baseLines, oursLines, theirsLines;

    if (useEditorCheck->isChecked() && editor)
    {
        oursLines = editor->exactLines();
    }
    else
    {
        oursLines = linesOf(oursEdit->text().trimmed());
    }
    baseLines = linesOf(baseEdit->text().trimmed());
    theirsLines = linesOf(theirsEdit->text().trimmed());

    // plain text appears right away; the aligned, colored diff replaces it when ready
    diffWidget->setPlainLines(baseLines, oursLines, theirsLines);

    // titles with file names where known
    const auto nameOf = [](const QString &path) {
        return path.isEmpty() ? QString() : QFileInfo(path).fileName();
    };
    diffWidget->setTitles(nameOf(baseEdit->text()),
                          useEditorCheck->isChecked() ? tr("Editor") : nameOf(oursEdit->text()),
                          nameOf(theirsEdit->text()));

    statsLabel->setText(tr("Computing diff…"));
    saveMergedButton->setEnabled(false);

    if (!calculator)
    {
        calculator = new DiffCalculator(this);
        connect(calculator, &DiffCalculator::threeWayReady, this,
                [this](quint64 generation, ThreeWayDiff::Result result) {
            if (generation != pendingGeneration_)
                return; // a newer compare was requested meanwhile

            result_ = std::move(result);
            diffWidget->setResult(result_);

            int conflicts = 0, oursChanges = 0, theirsChanges = 0, same = 0;
            for (const auto &region : result_.regions)
            {
                switch (region.kind)
                {
                case ThreeWayDiff::RowKind::Conflict: ++conflicts; break;
                case ThreeWayDiff::RowKind::ChangedOurs: ++oursChanges; break;
                case ThreeWayDiff::RowKind::ChangedTheirs: ++theirsChanges; break;
                case ThreeWayDiff::RowKind::SameChange: ++same; break;
                case ThreeWayDiff::RowKind::Unchanged: break;
                }
            }

            if (result_.regions.isEmpty())
                statsLabel->setText(tr("No differences - all three versions are identical."));
            else
                statsLabel->setText(tr("Changes: ours %1 | theirs %2 | same %3 | conflicts %4")
                                        .arg(oursChanges).arg(theirsChanges).arg(same).arg(conflicts));

            saveMergedButton->setEnabled(true);
        });
    }

    pendingGeneration_ = calculator->computeThreeWay(baseLines, oursLines, theirsLines);
}

void ThreeWayDiffDialog::saveMerged()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save the merged result"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        statsLabel->setText(tr("Could not write %1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << result_.mergedLines.join('\n');
    file.close();

    statsLabel->setText(tr("Merged result saved to %1").arg(QFileInfo(path).fileName()));
}
