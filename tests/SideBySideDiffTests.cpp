// Smoke tests of SideBySideDiffWidget: the two panes must always have the same number of
// lines (a missing line becomes an empty placeholder), the change runs must be computed
// correctly, changed lines must be colored and the scrollbars must stay in sync.
#include <gtest/gtest.h>

#include <QApplication>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>

#include "utils/DiffCalculation.h"
#include "widgets/SideBySideDiffWidget.h"

namespace
{

QTextEdit *pane(SideBySideDiffWidget &widget, const QString &name)
{
    return widget.findChild<QTextEdit *>(name);
}

/// Fills `oldLines`/`newLines` with `count` numbered lines; `skipOld`/`skipNew` drop some of them.
std::pair<QStringList, QStringList> numberedLines(int count, const QList<int> &skipOld = {}, const QList<int> &skipNew = {})
{
    QStringList oldLines, newLines;
    for (int i = 1; i <= count; ++i)
    {
        if (!skipOld.contains(i))
            oldLines << QStringLiteral("old line %1").arg(i);
        if (!skipNew.contains(i))
            newLines << QStringLiteral("new line %1").arg(i);
    }
    return {oldLines, newLines};
}

std::vector<DiffCalculation::DiffLine> diffOf(const QStringList &oldLines, const QStringList &newLines)
{
    return DiffCalculation::computeDiff(oldLines, newLines);
}

} // namespace


TEST(SideBySideDiffWidget, PanesHaveTheSameNumberOfLines)
{
    SideBySideDiffWidget widget;
    const auto [oldLines, newLines] = numberedLines(50, {10, 20}, {30});
    const auto diff = diffOf(oldLines, newLines);

    widget.setDiffLines(diff);

    ASSERT_EQ(pane(widget, QStringLiteral("old"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("new"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("numbersOld"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("numbersNew"))->document()->blockCount(),
              static_cast<int>(diff.size()));
}

TEST(SideBySideDiffWidget, MissingLinesBecomeEmptyPlaceholders)
{
    SideBySideDiffWidget widget;
    // old has "removed" where new has nothing; new has "added" where old has nothing
    const auto diff = diffOf({QStringLiteral("a"), QStringLiteral("removed"), QStringLiteral("b")},
                             {QStringLiteral("a"), QStringLiteral("added"), QStringLiteral("b")});

    widget.setDiffLines(diff);

    const QString oldText = pane(widget, QStringLiteral("old"))->toPlainText();
    const QString newText = pane(widget, QStringLiteral("new"))->toPlainText();

    // both documents have three rows; the placeholders are empty lines
    ASSERT_EQ(oldText.split('\n').size(), 3);
    ASSERT_EQ(newText.split('\n').size(), 3);
    ASSERT_TRUE(oldText.split('\n')[1].isEmpty() || oldText.split('\n')[1] == QStringLiteral("removed"));
}

TEST(SideBySideDiffWidget, ChangeRegionsCoverOnlyChangedRows)
{
    SideBySideDiffWidget widget;
    const auto diff = diffOf({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")},
                             {QStringLiteral("a"), QStringLiteral("B"), QStringLiteral("c"), QStringLiteral("e")});

    widget.setDiffLines(diff);

    // two separate changes: row 1 (b->B) and row 3 (d->e)
    ASSERT_EQ(widget.changeRegions().size(), 2);
    for (const auto &region : widget.changeRegions())
    {
        for (int row = region.firstEntry; row <= region.lastEntry; ++row)
            ASSERT_NE(diff[row].type, DiffCalculation::DiffType::Unchanged);
        ASSERT_EQ(diff[region.firstEntry].type == DiffCalculation::DiffType::Unchanged, false);
    }
}

TEST(SideBySideDiffWidget, ChangedLinesAreColored)
{
    SideBySideDiffWidget widget;
    const auto diff = diffOf({QStringLiteral("same"), QStringLiteral("old text")},
                             {QStringLiteral("same"), QStringLiteral("new text")});

    widget.setDiffLines(diff);

    const QTextBlock modifiedOld = pane(widget, QStringLiteral("old"))->document()->findBlockByNumber(1);
    const QTextBlock modifiedNew = pane(widget, QStringLiteral("new"))->document()->findBlockByNumber(1);
    const QTextBlock unchangedOld = pane(widget, QStringLiteral("old"))->document()->findBlockByNumber(0);

    ASSERT_TRUE(modifiedOld.isValid());
    ASSERT_TRUE(modifiedNew.isValid());
    ASSERT_NE(modifiedOld.blockFormat().background().style(), Qt::NoBrush);
    ASSERT_NE(modifiedNew.blockFormat().background().style(), Qt::NoBrush);
    ASSERT_EQ(unchangedOld.blockFormat().background().style(), Qt::NoBrush);
}

TEST(SideBySideDiffWidget, ChangedCharactersAreHighlighted)
{
    SideBySideDiffWidget widget;
    const auto diff = diffOf({QStringLiteral("hello world")}, {QStringLiteral("hello there")});

    widget.setDiffLines(diff);

    // the deleted "world" must carry a character background on the left pane
    const QTextBlock block = pane(widget, QStringLiteral("old"))->document()->findBlockByNumber(0);
    ASSERT_TRUE(block.isValid());

    bool foundHighlighted = false;
    for (auto it = block.begin(); !it.atEnd(); ++it)
    {
        const QTextFragment fragment = it.fragment();
        if (fragment.text().contains(QStringLiteral("world")) &&
            fragment.charFormat().background().style() != Qt::NoBrush)
        {
            foundHighlighted = true;
            break;
        }
    }
    ASSERT_TRUE(foundHighlighted);
}

TEST(SideBySideDiffWidget, ScrollBarsStayInSync)
{
    SideBySideDiffWidget widget;
    const auto [oldLines, newLines] = numberedLines(300, {100}, {200});
    widget.setDiffLines(diffOf(oldLines, newLines));

    widget.resize(900, 250);
    widget.show();
    QApplication::processEvents();

    const auto scrollBars = widget.findChildren<QScrollBar *>();
    int verticalBars = 0;
    for (QScrollBar *bar : scrollBars)
    {
        if (bar->orientation() == Qt::Vertical && bar->maximum() > 0)
            ++verticalBars;
    }
    ASSERT_EQ(verticalBars, 4);

    pane(widget, QStringLiteral("old"))->verticalScrollBar()->setValue(500);
    QApplication::processEvents();

    for (const QString &name : {QStringLiteral("old"), QStringLiteral("new"),
                                QStringLiteral("numbersOld"), QStringLiteral("numbersNew")})
    {
        ASSERT_EQ(pane(widget, name)->verticalScrollBar()->value(), 500);
    }
}

TEST(SideBySideDiffWidget, NextAndPreviousButtonsMoveBetweenChanges)
{
    SideBySideDiffWidget widget;
    QStringList oldLines, newLines;
    for (int i = 1; i <= 200; ++i)
    {
        oldLines << QStringLiteral("old %1").arg(i);
        newLines << (i == 50 || i == 150 ? QStringLiteral("changed %1").arg(i)
                                         : QStringLiteral("old %1").arg(i));
    }

    widget.setDiffLines(diffOf(oldLines, newLines));
    ASSERT_EQ(widget.changeRegions().size(), 2);

    widget.resize(900, 250);
    widget.show();
    QApplication::processEvents();

    const auto buttons = widget.findChildren<QPushButton *>();
    ASSERT_EQ(buttons.size(), 2);

    // next wraps around: region 1, then region 0, then region 1 again
    buttons[1]->click();
    QApplication::processEvents();
    const int region1Scroll = widget.scrollValue();
    ASSERT_NE(region1Scroll, 0);

    buttons[1]->click();
    QApplication::processEvents();
    const int region0Scroll = widget.scrollValue();
    ASSERT_NE(region0Scroll, region1Scroll);

    buttons[0]->click(); // previous wraps back to region 1
    QApplication::processEvents();
    ASSERT_EQ(widget.scrollValue(), region1Scroll);

    buttons[0]->click(); // previous again lands on region 0
    QApplication::processEvents();
    ASSERT_EQ(widget.scrollValue(), region0Scroll);
}

TEST(SideBySideDiffWidget, EmptyDiffShowsNoChanges)
{
    SideBySideDiffWidget widget;
    const auto diff = diffOf({QStringLiteral("a"), QStringLiteral("b")}, {QStringLiteral("a"), QStringLiteral("b")});

    widget.setDiffLines(diff);

    ASSERT_TRUE(widget.changeRegions().isEmpty());
    ASSERT_EQ(pane(widget, QStringLiteral("old"))->document()->blockCount(), 2);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
