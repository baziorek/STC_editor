#include "cppcompilerdialog.h"
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QFile>
#include <QMessageBox>


CppCompilerDialog::CppCompilerDialog(const QString& code, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("g++ Compilation Output");
    resize(700, 500);

    outputEdit = new QTextEdit(this);
    outputEdit->setReadOnly(true);
    closeButton = new QPushButton("Close", this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(outputEdit);
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    compileCode(code);
}

void CppCompilerDialog::compileCode(const QString& code)
{
    // Create temporary file with source code
    QTemporaryFile sourceFile(QDir::tempPath() + "/codeXXXXXX.cpp");
    if (!sourceFile.open())
    {
        outputEdit->setText("Failed to create temporary file.");
        return;
    }

    sourceFile.write(code.toUtf8());
    sourceFile.flush();

    QString exeFile = sourceFile.fileName() + ".out";

    QProcess compiler;
    QStringList args;
    args << sourceFile.fileName() << "--std=c++23" << "-o" << exeFile;

    compiler.start("g++", args);
    if (!compiler.waitForFinished(5000))
    {
        outputEdit->setText("Compilation timed out or failed to start.");
        return;
    }

    QString output = compiler.readAllStandardOutput();
    QString errors = compiler.readAllStandardError();

    QString fullOutput;
    if (!output.isEmpty())
    {
        fullOutput += "Standard Output:\n" + output + "\n";
    }
    if (!errors.isEmpty())
    {
        fullOutput += "Compiler Errors/Warnings:\n" + errors;
    }

    if (fullOutput.isEmpty())
    {
        fullOutput = "Compilation succeeded with no output.";
    }

    outputEdit->setPlainText(fullOutput);
}
