#pragma once

#include <QWidget>

namespace Ui {
class FindDialog;
}

class QTreeWidgetItem;

class CodeEditor;

class FindDialog : public QWidget
{
    Q_OBJECT

    CodeEditor* codeEditor{};
    bool replaceMode{};

public:
    struct MatchStats
    {
        int insensitive = {};
        int insensitiveWhole = {};
        int sensitive = {};
        int sensitiveWhole = {};

        bool isZero() const
        {
            return 0 == insensitive && 0 == insensitiveWhole && 0 == sensitive && 0 == sensitiveWhole;
        }
    };

    explicit FindDialog(QWidget *parent = nullptr);
    ~FindDialog();

    void setCodeEditor(CodeEditor* codeEditor)
    {
        this->codeEditor = codeEditor;
    }

    void setReplaceMode(bool enabled);
    bool isReplaceMode() const;
    void focusInput(bool preferReplacementField = false);

signals:
    void jumpToLocationRequested(int line, int offset);

public slots:
    void currentTextChanged(QString newText);
    void onResultItemClicked(QTreeWidgetItem* item, int column);

    void odCheckboxMatchCasesChanged(bool checked);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

    void installEventFilter2HandleMovingBetweenOccurences();

    // Event filter to handle Enter and Shift+Enter in the search field
    bool eventFilter(QObject* obj, QEvent* event) override;

    MatchStats showOccurences(const QString& text);

    void updateHighlights();
    bool isMatchAlreadyReplacement(const QString& documentText, int matchStart, int matchLength) const;
    void updateReplacementControls();

private slots:
    void onNextOccurencyPressed();
    void onPreviousOccurencyPressed();
    void onReplacementCriteriaChanged();
    void onSelectAllMatchesPressed();
    void onDeselectAllMatchesPressed();
    void onReplaceSelectedMatchesPressed();
    void onResultItemChanged(QTreeWidgetItem* item, int column);

private:
    Ui::FindDialog *ui;
};
