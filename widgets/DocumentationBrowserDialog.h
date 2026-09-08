#pragma once

#include <QDialog>
#include <QUrl>

class DocumentationBrowserDialog final : public QDialog
{
public:
    explicit DocumentationBrowserDialog(const QUrl& initialUrl, const QString& windowTitle,
                                        QWidget* parent = nullptr);
};
