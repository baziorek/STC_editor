#pragma once

#include <QDialog>
#include <QString>

class QUrl;
class QWidget;

class DocumentationBrowserDialog final : public QDialog
{
public:
    explicit DocumentationBrowserDialog(const QUrl& initialUrl, const QString& windowTitle,
                                        QWidget* parent = nullptr);
};
