#pragma once

#include <QList>
#include <QVector>
#include <QWidget>

#include "utils/ThreeWayDiff.h"

class QLabel;
class QPushButton;
class QTextEdit;

namespace Ui
{
class ThreeWayDiffWidget;
}

/**
 * @class ThreeWayDiffWidget
 * @brief A read-only diff3-style view: base, ours and theirs side by side.
 *
 * Every display row carries one line of each side (empty where the side has none),
 * so the three panes always scroll together. Rows are colored by what happened:
 * a change only on our side is green on ours, only on theirs is blue on theirs,
 * the same change on both is yellow everywhere and a conflict is red on both
 * changed sides. The header offers previous/next conflict navigation.
 */
class ThreeWayDiffWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeWayDiffWidget(QWidget *parent = nullptr);
    ~ThreeWayDiffWidget() override;

    void setResult(const ThreeWayDiff::Result &result);
    /// Shows the three texts right away, before the diff is computed (no colors).
    /// Replaced by setResult() when the computation is ready.
    void setPlainLines(const QStringList &baseLines, const QStringList &oursLines,
                       const QStringList &theirsLines);
    void setTitles(const QString &baseTitle, const QString &oursTitle, const QString &theirsTitle);

    const ThreeWayDiff::Result &result() const { return result_; }

private:
    void fillPanes(const QString &baseText, const QString &oursText, const QString &theirsText,
                   const QString &baseNumbers, const QString &oursNumbers,
                   const QString &theirsNumbers);
    void applyRowFormats();
    void syncScrollBars();
    void updateConflictLabel();
    void scrollToRegion(int regionIndex);
    void nextConflict();
    void previousConflict();

    ThreeWayDiff::Result result_;
    int currentConflict_ = -1;

    Ui::ThreeWayDiffWidget *ui = nullptr;

    QLabel *baseTitleLabel_ = nullptr;
    QLabel *oursTitleLabel_ = nullptr;
    QLabel *theirsTitleLabel_ = nullptr;
    QLabel *conflictLabel_ = nullptr;
    QPushButton *previousButton_ = nullptr;
    QPushButton *nextButton_ = nullptr;

    QTextEdit *baseNumbersEdit_ = nullptr;
    QTextEdit *baseEdit_ = nullptr;
    QTextEdit *oursNumbersEdit_ = nullptr;
    QTextEdit *oursEdit_ = nullptr;
    QTextEdit *theirsNumbersEdit_ = nullptr;
    QTextEdit *theirsEdit_ = nullptr;
};
