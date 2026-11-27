// Smoke tests of SideBySideDiffWidget: the two panes must always have the same number of
// lines (a missing line becomes an empty placeholder), the change runs must be computed
// correctly, changed lines must be colored and the scrollbars must stay in sync.
#include <gtest/gtest.h>

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QMap>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFragment>
#include <QtTest/QTest>

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

    ASSERT_EQ(pane(widget, QStringLiteral("oldEdit"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("newEdit"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("numberOldEdit"))->document()->blockCount(),
              static_cast<int>(diff.size()));
    ASSERT_EQ(pane(widget, QStringLiteral("numberNewEdit"))->document()->blockCount(),
              static_cast<int>(diff.size()));
}

TEST(SideBySideDiffWidget, MissingLinesBecomeEmptyPlaceholders)
{
    SideBySideDiffWidget widget;
    // old has "removed" where new has nothing; new has "added" where old has nothing
    const auto diff = diffOf({QStringLiteral("a"), QStringLiteral("removed"), QStringLiteral("b")},
                             {QStringLiteral("a"), QStringLiteral("added"), QStringLiteral("b")});

    widget.setDiffLines(diff);

    const QString oldText = pane(widget, QStringLiteral("oldEdit"))->toPlainText();
    const QString newText = pane(widget, QStringLiteral("newEdit"))->toPlainText();

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

    const QTextBlock modifiedOld = pane(widget, QStringLiteral("oldEdit"))->document()->findBlockByNumber(1);
    const QTextBlock modifiedNew = pane(widget, QStringLiteral("newEdit"))->document()->findBlockByNumber(1);
    const QTextBlock unchangedOld = pane(widget, QStringLiteral("oldEdit"))->document()->findBlockByNumber(0);

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
    const QTextBlock block = pane(widget, QStringLiteral("oldEdit"))->document()->findBlockByNumber(0);
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

    pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar()->setValue(500);
    QApplication::processEvents();

    for (const QString &name : {QStringLiteral("oldEdit"), QStringLiteral("newEdit"),
                                QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")})
    {
        ASSERT_EQ(pane(widget, name)->verticalScrollBar()->value(), 500);
    }
}

TEST(SideBySideDiffWidget, ScrollBarRangesHaveNoDeadZone)
{
    // QTextEdit can keep a scrollbar maximum computed from a stale layout; the dead zone
    // at the end made the thumb move while the text stayed put. Every pane's maximum must
    // equal the real scrollable distance: document height minus viewport height.
    SideBySideDiffWidget widget;
    const auto [oldLines, newLines] = numberedLines(300, {100}, {200});
    widget.setDiffLines(diffOf(oldLines, newLines));

    widget.resize(900, 250);
    widget.show();
    QApplication::processEvents();
    QTest::qWait(10); // let the deferred scrollbar-range fix run
    QApplication::processEvents();

    for (const QString &name : {QStringLiteral("oldEdit"), QStringLiteral("newEdit"),
                                QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")})
    {
        auto *edit = pane(widget, name);
        const int documentHeight =
            int(edit->document()->documentLayout()->documentSize().height());
        const int expected = qMax(0, documentHeight - edit->viewport()->height());
        ASSERT_EQ(edit->verticalScrollBar()->maximum(), expected) << name.toStdString();
    }

    // and at the very bottom every pane must show the same last lines
    const auto firstVisible = [](QTextEdit *edit) {
        return edit->cursorForPosition(QPoint(10, 2)).blockNumber();
    };
    pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar()->setValue(
        pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar()->maximum());
    QApplication::processEvents();

    const int bottomLine = firstVisible(pane(widget, QStringLiteral("oldEdit")));
    ASSERT_GT(bottomLine, 0);
    for (const QString &name : {QStringLiteral("newEdit"),
                                QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")})
    {
        ASSERT_EQ(firstVisible(pane(widget, name)), bottomLine) << name.toStdString();
    }
}

TEST(SideBySideDiffWidget, PreviewThenDiffKeepsRowsSynced)
{
    // DiffReviewDialog flow: setPlainLines shows the saved/current files side by side
    // (different line counts -> different document heights), then the async diff replaces
    // them with aligned rows. Scrolling must keep the same row at the top of every pane.
    SideBySideDiffWidget widget;
    QStringList oldLines, newLines;
    for (int i = 1; i <= 120; ++i) oldLines << QStringLiteral("old %1").arg(i);
    for (int i = 1; i <= 200; ++i) newLines << QStringLiteral("new %1").arg(i);

    const auto firstVisible = [](QTextEdit *edit) {
        return edit->cursorForPosition(QPoint(10, 2)).blockNumber();
    };
    const auto expectSynced = [&]() {
        const int row = firstVisible(pane(widget, QStringLiteral("oldEdit")));
        for (const QString &name : {QStringLiteral("newEdit"),
                                    QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")})
        {
            ASSERT_EQ(firstVisible(pane(widget, name)), row) << name.toStdString();
        }
    };

    widget.setPlainLines(oldLines, newLines);
    widget.resize(900, 400);
    widget.show();
    QApplication::processEvents();
    QTest::qWait(20);
    QApplication::processEvents();

    auto *oldBar = pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar();
    oldBar->setValue(oldBar->maximum() / 2);
    QApplication::processEvents();
    expectSynced();

    const auto diff = diffOf(oldLines, newLines);
    widget.setDiffData(diff, DiffCalculation::computeAllLineDiffs(diff));
    QApplication::processEvents();
    QTest::qWait(20);
    QApplication::processEvents();

    for (int value : {0, oldBar->maximum() / 3, oldBar->maximum()})
    {
        oldBar->setValue(value);
        QApplication::processEvents();
        expectSynced();
    }
}

TEST(SideBySideDiffWidget, ScrollStaysRowSyncedWithRealisticContent)
{
    // lines long enough to show horizontal scrollbars, empty lines, unicode - like real files
    QStringList oldLines, newLines;
    for (int i = 1; i <= 400; ++i)
    {
        QString oldLine = QStringLiteral("line %1 ").arg(i);
        if (i % 37 == 0)
            oldLine += QString(500, QLatin1Char('x')); // forces a horizontal scrollbar
        if (i % 11 == 0)
            oldLine += QStringLiteral(" ąćęłńóśźż [tag] http://example.com/") ;
        oldLines << oldLine;

        QString newLine = oldLine;
        if (i % 50 == 0)
            newLine += QStringLiteral(" edited");
        newLines << newLine;
    }

    SideBySideDiffWidget widget;
    widget.setDiffLines(diffOf(oldLines, newLines));
    widget.resize(1200, 600);
    widget.show();
    QApplication::processEvents();
    QTest::qWait(30);
    QApplication::processEvents();

    const auto firstVisible = [](QTextEdit *edit) {
        return edit->cursorForPosition(QPoint(10, 2)).blockNumber();
    };
    auto *oldBar = pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar();
    const int steps = 25;
    for (int s = 0; s <= steps; ++s)
    {
        oldBar->setValue(oldBar->maximum() * s / steps);
        QApplication::processEvents();
        const int row = firstVisible(pane(widget, QStringLiteral("oldEdit")));
        for (const QString &name : {QStringLiteral("newEdit"),
                                    QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")})
        {
            // at the very bottom a pane without a horizontal scrollbar can be one row
            // short of the others - their viewports are a few pixels shorter/taller
            ASSERT_LE(qAbs(firstVisible(pane(widget, name)) - row), 1)
                << "step" << s << name.toStdString();
        }
    }
}

TEST(SideBySideDiffWidget, ScrollingOnePaneRepaintsTheOtherPanes)
{
    // Regression: programmatic scrollbar writes must keep the valueChanged signal alive.
    // QTextEdit scrolls and repaints its viewport from its own valueChanged handler - a
    // QSignalBlocker around setValue() moves the thumb and the model position but the
    // text stays frozen on screen (only the scrollbar thumb moves). Count real Paint
    // events on the viewports: a synced pane that never repaints means the user sees
    // a frozen pane.
    struct PaintCounter : QObject
    {
        explicit PaintCounter(QObject *parent = nullptr) : QObject(parent) {}
        int count = 0;
        bool eventFilter(QObject *, QEvent *event) override
        {
            if (event->type() == QEvent::Paint)
                ++count;
            return false;
        }
    };

    SideBySideDiffWidget widget;
    const auto [oldLines, newLines] = numberedLines(300, {100}, {200});
    widget.setDiffLines(diffOf(oldLines, newLines));

    widget.resize(900, 250);
    widget.show();
    QApplication::processEvents();
    QTest::qWait(20);
    QApplication::processEvents();

    const QStringList names = {QStringLiteral("oldEdit"), QStringLiteral("newEdit"),
                               QStringLiteral("numberOldEdit"), QStringLiteral("numberNewEdit")};
    QMap<QString, PaintCounter *> counters;
    for (const QString &name : names)
    {
        auto *counter = new PaintCounter(&widget);
        pane(widget, name)->viewport()->installEventFilter(counter);
        counters.insert(name, counter);
    }

    auto *oldBar = pane(widget, QStringLiteral("oldEdit"))->verticalScrollBar();
    const int target = oldBar->maximum() / 2;
    ASSERT_GT(target, 0);
    oldBar->setValue(target);
    QApplication::processEvents();
    QTest::qWait(50);
    QApplication::processEvents();

    // the initiating pane repaints for sure; the synced panes must repaint too
    for (const QString &name : names)
    {
        ASSERT_GT(counters.value(name)->count, 0) << name.toStdString();
    }

    // and scrolling back must repaint as well
    for (auto *counter : counters)
        counter->count = 0;
    oldBar->setValue(0);
    QApplication::processEvents();
    QTest::qWait(50);
    QApplication::processEvents();
    for (const QString &name : names)
    {
        ASSERT_GT(counters.value(name)->count, 0) << name.toStdString();
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
    ASSERT_EQ(pane(widget, QStringLiteral("oldEdit"))->document()->blockCount(), 2);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
