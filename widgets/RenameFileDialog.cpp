#include <QFileInfo>
#include <QDir>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QRegularExpression>
#include "RenameFileDialog.h"
#include "CodeEditor.h"
#include "ui_RenameFileDialog.h"


RenameFileDialog::RenameFileDialog(CodeEditor *textEditor, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RenameFileDialog), textEditor{textEditor}
{
    ui->setupUi(this);

    const auto oldFilePath = textEditor->getFileName();

    QFileInfo fileInfo(oldFilePath);
    // Wyboldowany plik, zwykła ścieżka
    QString styled = QString("<span style='color:gray'>%1/</span><b>%2</b>")
                         .arg(fileInfo.dir().absolutePath(), fileInfo.fileName());
    ui->oldFilePathLabel->setText(styled);
    ui->oldFilePathLabel->setTextFormat(Qt::RichText);

    ui->filePathLineEdit->setText(fileInfo.dir().absolutePath());
    ui->fileNameLineEdit->setText(fileInfo.fileName());
    ui->overwriteCheckBox->setChecked(false);
    ui->overwriteCheckBox->setVisible(false); // Pokazuj tylko, gdy plik istnieje
    ui->targetFileInfoLabel->clear();

    connect(ui->filePathLineEdit, &QLineEdit::textChanged, this, &RenameFileDialog::validateAndUpdateUi);
    connect(ui->fileNameLineEdit, &QLineEdit::textChanged, this, &RenameFileDialog::validateAndUpdateUi);
    connect(ui->createDirCheckBox, &QCheckBox::checkStateChanged, this, &RenameFileDialog::validateAndUpdateUi);
    connect(ui->overwriteCheckBox, &QCheckBox::checkStateChanged, this, &RenameFileDialog::validateAndUpdateUi);
    connect(this, &QDialog::accepted, this, &RenameFileDialog::tryRenameFile);

    validateAndUpdateUi();
}

RenameFileDialog::~RenameFileDialog()
{
    delete ui;
}

QString RenameFileDialog::newFilePath() const
{
    return ui->filePathLineEdit->text();
}

QString RenameFileDialog::newFileName() const
{
    return ui->fileNameLineEdit->text();
}

QString RenameFileDialog::newAbsoluteFilePath() const
{
    QDir dir(ui->filePathLineEdit->text());
    return dir.filePath(ui->fileNameLineEdit->text());
}

bool RenameFileDialog::createDirectoryChecked() const
{
    return ui->createDirCheckBox->isChecked();
}

void RenameFileDialog::validateAndUpdateUi()
{
    // Reset stylów i tooltipów
    auto clearError = [](QWidget* w) {
        w->setStyleSheet("");
        w->setToolTip("");
    };
    clearError(ui->filePathLineEdit);
    clearError(ui->fileNameLineEdit);
    ui->targetFileInfoLabel->clear();

    QString dirPath = ui->filePathLineEdit->text();
    QString fileName = ui->fileNameLineEdit->text();
    QString absPath = newAbsoluteFilePath();
    bool ok = true;

    QFileInfo dirInfo(dirPath);
    QFileInfo fileInfo(absPath);
    bool dirExists = dirInfo.exists() && dirInfo.isDir();
    bool fileExists = fileInfo.exists();
    bool canWrite = dirExists && QFileInfo(dirPath).isWritable();
    bool createDir = ui->createDirCheckBox->isChecked();

    // Walidacja katalogu
    if (!dirExists && !createDir)
    {
        ui->filePathLineEdit->setStyleSheet("border: 2px solid red");
        ui->filePathLineEdit->setToolTip(tr("Katalog nie istnieje. Zaznacz opcję tworzenia katalogu lub podaj istniejący."));
        ok = false;
    }
    else if (dirExists && !canWrite)
    {
        ui->filePathLineEdit->setStyleSheet("border: 2px solid red");
        ui->filePathLineEdit->setToolTip(tr("Brak uprawnień do zapisu w tym katalogu."));
        ok = false;
    }

    // Walidacja nazwy pliku
    if (fileName.trimmed().isEmpty())
    {
        ui->fileNameLineEdit->setStyleSheet("border: 2px solid red");
        ui->fileNameLineEdit->setToolTip(tr("Nazwa pliku nie może być pusta."));
        ok = false;
    }

    // Informacja o pliku docelowym
    if (fileExists)
    {
        ui->overwriteCheckBox->setVisible(true);
        QFileInfo fi(absPath);
        QString info = tr("Uwaga: plik istnieje. Rozmiar: %1 B, modyfikacja: %2")
            .arg(fi.size())
            .arg(fi.lastModified().toString(Qt::ISODate));
        ui->targetFileInfoLabel->setText(info);
        if (!ui->overwriteCheckBox->isChecked()) {
            ui->fileNameLineEdit->setStyleSheet("border: 2px solid red");
            ui->fileNameLineEdit->setToolTip(tr("Plik o tej nazwie już istnieje. Zaznacz opcję zastępowania."));
            ok = false;
        }
    }
    else
    {
        ui->overwriteCheckBox->setVisible(false);
        ui->targetFileInfoLabel->clear();
    }

    // Blokada przycisku OK
    auto btnBox = ui->buttonBox;
    if (btnBox)
    {
        auto okBtn = btnBox->button(QDialogButtonBox::Ok);
        if (okBtn) okBtn->setEnabled(ok);
    }
}

void RenameFileDialog::tryRenameFile()
{
    // Ostateczna walidacja
    validateAndUpdateUi();
    if (ui->buttonBox->button(QDialogButtonBox::Ok) && !ui->buttonBox->button(QDialogButtonBox::Ok)->isEnabled())
    {
        // Nie pozwól zamknąć dialogu jeśli są błędy
        return;
    }

    QString oldPath = ui->oldFilePathLabel->text();
    QString newPath = newAbsoluteFilePath();
    QFileInfo fi(newPath);
    bool createDir = ui->createDirCheckBox->isChecked();
    bool overwrite = ui->overwriteCheckBox->isChecked();

    // Tworzenie katalogu jeśli trzeba
    QDir dir(ui->filePathLineEdit->text());
    if (! dir.exists() && createDir)
    {
        if (!dir.mkpath("."))
        {
            ui->filePathLineEdit->setStyleSheet("border: 2px solid red");
            ui->filePathLineEdit->setToolTip(tr("Nie udało się utworzyć katalogu."));
            return;
        }
    }

    // Rename
    QFile src(getOldAbsoluteFilePath());
    if (src.fileName() == newPath)
    {
        emit renameCompleted(newPath);
        return;
    }
    if (fi.exists() && !overwrite)
    {
        return;
    }

    textEditor->stopWatchingFiles();

    if (! src.rename(newPath))
    {
        ui->fileNameLineEdit->setStyleSheet("border: 2px solid red");
        ui->fileNameLineEdit->setToolTip(tr("Zmiana nazwy pliku nie powiodła się."));

        textEditor->enableWatchingOfFile(textEditor->getFileName());

        return;
    }

    textEditor->setFileName(newPath);
    textEditor->enableWatchingOfFile(newPath);

    emit renameCompleted(newPath);
}

QString RenameFileDialog::getOldAbsoluteFilePath() const
{
    // Stara ścieżka jest w labelu w formacie HTML, wyciągnij ją:
    QString html = ui->oldFilePathLabel->text();
    QRegularExpression re("<span style='color:gray'>(.*)/</span><b>(.*)</b>");
    auto match = re.match(html);
    if (match.hasMatch())
    {
        return match.captured(1) + "/" + match.captured(2);
    }
    return html;
}
