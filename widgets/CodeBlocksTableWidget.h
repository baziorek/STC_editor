
#pragma once

#include <QTableWidget>
#include <QMenu>
#include <QString>
#include <QMap>

class CodeEditor;

class CodeBlocksTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    void createFilterMenu();

    explicit CodeBlocksTableWidget(QWidget* parent = nullptr);

    void setTextEditor(CodeEditor* newTextEditor);
    CodeEditor* getTextEditor() const { return textEditor; }

public slots:
    void updateCodeBlocks();

private slots:
    void onCellClicked(int row, int column);
    void updateFilterMenu();
    void applyFilter();

protected:
    void showEvent(QShowEvent* event) override;

private:
    // Convert code type to display name (e.g. "code src="C++"" -> "code C++")
    QString getDisplayNameFromCodeType(const QString& tag, const QString& language) const;
    
    QString getFilterCategory4CodeBlock(const QString& tag, const QString& language) const;

    void setupTable();

    CodeEditor* textEditor{nullptr};
    QMenu* filterMenu{nullptr};
    QMap<QString, bool> filterStates4EachCategory;

    enum Columns
    {
        Position = 0,
        Type = 1,
        ColumnCount
    };

    // Filter categories
    const QString FILTER_CPP = "C++";
    const QString FILTER_PYTHON = "Python";
    const QString FILTER_GENERAL = "General";
};