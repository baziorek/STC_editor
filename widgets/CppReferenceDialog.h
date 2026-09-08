#pragma once

#include <QDialog>

class CppReferenceDialog final : public QDialog
{
public:
    explicit CppReferenceDialog(const QString& symbol, QWidget* parent = nullptr);
};
