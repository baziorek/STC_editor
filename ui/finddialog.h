#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextCursor>
#include <QTimer>
#include <QWidget>
#include <Qt>

#include "utils/LineSearch.h"

namespace Ui {
class FindDialog;
}

class QEvent;
class QHideEvent;
class QShowEvent;
class QTreeWidgetItem;

class CodeEditor;
class HighlightDelegate;

/** @brief The find (and replace) widget with the list of found places
 *
 * While the widget is visible the list follows the document: each edit (typing, a new line, undo, a replacement...)
 * refreshes the places, so line numbers and offsets are always the current ones. A hidden widget does not follow
 * the document at all, it searches again when it is shown.
 *
 * A text with several words (`Ala ma koty`) finds the lines which contain all of them in any order. This can be
 * switched off with the "Any order" check box (then the whole text is searched as it is), and it is never used
 * when replacing. */
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
        int lines = {};         ///< lines with all the words, when the words are searched in any order
        bool anyOrder = {};     ///< `lines` is the result, not the four numbers above

        bool isZero() const
        {
            return 0 == insensitive && 0 == insensitiveWhole && 0 == sensitive && 0 == sensitiveWhole && 0 == lines;
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
    struct SearchOutcome;

    // Searching
    void rebuildResults(const QString& searchText);
    void collectExactOccurrences(const QString& searchText, SearchOutcome& outcome);
    void collectLinesWithAllWords(const QStringList& words, SearchOutcome& outcome);
    LineSearch::Criteria currentCriteria() const;
    bool isAnyOrderSearch() const;
    int maxContextLength() const;
    QString occurrencesLabelText(const MatchStats& stats) const;

    // Following the document while the widget is visible
    void startFollowingDocument();
    void stopFollowingDocument();
    void scheduleRefresh();
    void refreshResults();
    int refreshDelayMs() const;

    // Keeping the choices of the user over the refreshes (cursors follow the edits of the document)
    void rememberCurrentMatch(QTreeWidgetItem* current);
    void restoreCurrentResult();
    void rememberUncheckedMatches();
    void forgetUncheckedMatches();
    QSet<int> uncheckedMatchPositions() const;
    void setCheckStateOfAllMatches(Qt::CheckState state);

    Ui::FindDialog *ui;
    HighlightDelegate* resultsDelegate{};

    QTimer refreshTimer;                              ///< waits until the editing pauses, then refreshes the results
    QMetaObject::Connection documentChangesConnection;///< exists only while the widget is visible
    int lastSearchMs{};                               ///< how long the last search took, to not slow down the editing

    QTextCursor currentMatchStart;                    ///< where the match which the user went to last begins
    QList<QTextCursor> uncheckedMatchStarts;          ///< where the matches unticked in the replace mode begin
};
