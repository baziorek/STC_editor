#include "DiffCalculator.h"

DiffCalculator::DiffCalculator(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<std::vector<DiffCalculation::DiffLine>>();
    qRegisterMetaType<QList<DiffCalculation::LineDiffResult>>();
    qRegisterMetaType<ThreeWayDiff::Result>();

    moveToThread(&thread_);
    thread_.start();
}

DiffCalculator::~DiffCalculator()
{
    thread_.quit();
    thread_.wait();
}

quint64 DiffCalculator::computeTwoWay(const QStringList &oldLines, const QStringList &newLines)
{
    const quint64 generation = ++generation_;
    QMetaObject::invokeMethod(this, [this, generation, oldLines, newLines]() {
        auto diff = DiffCalculation::computeDiff(oldLines, newLines);
        auto lineDiffs = DiffCalculation::computeAllLineDiffs(diff);
        emit twoWayReady(generation, std::move(diff), std::move(lineDiffs));
    }, Qt::QueuedConnection);
    return generation;
}

quint64 DiffCalculator::computeThreeWay(const QStringList &base, const QStringList &ours,
                                        const QStringList &theirs)
{
    const quint64 generation = ++generation_;
    QMetaObject::invokeMethod(this, [this, generation, base, ours, theirs]() {
        emit threeWayReady(generation, ThreeWayDiff::compute(base, ours, theirs));
    }, Qt::QueuedConnection);
    return generation;
}
