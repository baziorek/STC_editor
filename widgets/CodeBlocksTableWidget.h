
#pragma once

#include <QTableWidget>
#include <QMenu>
#include <QString>
#include <QMap>

class CodeEditor;
class CodeBlock;

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
    QString getCodeWithoutTags(const CodeBlock& block) const;

    bool event(QEvent* event) override;

    void addCodeBlockLocationToTable(int row, const CodeBlock& block);
    void addCodeToTable(int row, const CodeBlock& block);
    void addCodeTypeToTable(int row, const CodeBlock& block);

private:
    // Convert code type to display name (e.g. "code src="C++"" -> "code C++")
    QString getDisplayNameFromCodeType(const QString& tag, const QString& language) const;
    
    QString getFilterCategory4CodeBlock(const QString& tag, const QString& language) const;

    void setupTable();

    CodeEditor* textEditor{nullptr};
    QMenu* filterMenu{nullptr};
    QMap<QString, bool> filterStates4EachCategory;

    // Filter categories
    const QString FILTER_CPP = "C++";
    const QString FILTER_PYTHON = "Python";
    const QString FILTER_GENERAL = "General";
};