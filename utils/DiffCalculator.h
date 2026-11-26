#pragma once

#include <QList>
#include <QObject>
#include <QThread>
#include <QStringList>
#include <vector>

#include "utils/DiffCalculation.h"
#include "utils/ThreeWayDiff.h"

Q_DECLARE_METATYPE(std::vector<DiffCalculation::DiffLine>);
Q_DECLARE_METATYPE(QList<DiffCalculation::LineDiffResult>);
Q_DECLARE_METATYPE(ThreeWayDiff::Result);

/**
 * @class DiffCalculator
 * @brief Runs the expensive diff computations on a worker thread.
 *
 * computeDiff() and computeAllLineDiffs() (and the three-way merge) can take a
 * noticeable time on large files; running them here keeps the GUI responsive.
 * Every call gets a generation number - a result with an outdated generation is
 * the caller's signal to ignore it (the user asked for a newer diff meanwhile).
 */
class DiffCalculator : public QObject
{
    Q_OBJECT

public:
    explicit DiffCalculator(QObject *parent = nullptr);
    ~DiffCalculator() override;

    /// Queues a two-way line diff + character diffs of the changed lines. Returns the
    /// generation number the matching twoWayReady() signal will carry.
    quint64 computeTwoWay(const QStringList &oldLines, const QStringList &newLines);
    /// Queues a three-way (diff3-style) computation. Returns the generation number.
    quint64 computeThreeWay(const QStringList &base, const QStringList &ours, const QStringList &theirs);

    /// The generation the NEXT compute call will return. Setting a dialog's pending
    /// generation to this value invalidates every result still in flight.
    quint64 nextGeneration() const { return generation_ + 1; }

signals:
    void twoWayReady(quint64 generation, std::vector<DiffCalculation::DiffLine> diff,
                     QList<DiffCalculation::LineDiffResult> lineDiffs);
    void threeWayReady(quint64 generation, ThreeWayDiff::Result result);

private:
    QThread thread_;
    quint64 generation_ = 0;
};
