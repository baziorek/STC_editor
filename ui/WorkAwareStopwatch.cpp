#include "WorkAwareStopwatch.h"
#include "ui_WorkAwareStopwatch.h"


WorkAwareStopwatch::WorkAwareStopwatch(QWidget *parent)
    : QGroupBox(parent)
    , ui(new Ui::WorkAwareStopwatch)
{
    ui->setupUi(this);
    ui->uptimeTimeEdit->setTimeZone(QTimeZone::systemTimeZone());
    ui->workingTimeTimeEdit->setTimeZone(QTimeZone::systemTimeZone());

    connect(&checkIdleTimer, &QTimer::timeout, this, &WorkAwareStopwatch::onUpdate);
    checkIdleTimer.setInterval(idleTresholdInMinutes());

    start();
}

WorkAwareStopwatch::~WorkAwareStopwatch()
{
    delete ui;
}

void WorkAwareStopwatch::start()
{
    appStartTime = QDateTime::currentDateTime();
    totalTimer.start();
    workTimer.start();
    lastWorkTime = appStartTime;
    working = false;
    accumulatedWorkMs = 0;
    checkIdleTimer.start();

    ui->uptimeTimeEdit->setToolTip("Started at: " + appStartTime.toString(Qt::ISODate));
    ui->uptimeTimeEdit->setTime(QTime(0, 0, 0));
    ui->workingTimeTimeEdit->setTime(QTime(0, 0, 0));

    notifyWorkActivity();
}


void WorkAwareStopwatch::stop()
{
    checkIdleTimer.stop();
}

void WorkAwareStopwatch::reset()
{
    totalTimer.restart();
    accumulatedWorkMs = 0;
    workTimer.restart();
    lastWorkTime = QDateTime::currentDateTime();
    working = false;

    ui->uptimeTimeEdit->setTime(QTime(0, 0, 0));
    ui->workingTimeTimeEdit->setTime(QTime(0, 0, 0));
    ui->uptimeTimeEdit->setToolTip({});
}

void WorkAwareStopwatch::notifyWorkActivity()
{
    lastWorkTime = QDateTime::currentDateTime();
    if (! working)
    {
        workTimer.restart();
        working = true;
    }
}

void WorkAwareStopwatch::onUpdate()
{
    constexpr qint64 msecsPerMinute = 1000 * 60;

    const auto now = QDateTime::currentDateTime();

    if (working && lastWorkTime.msecsTo(now) > idleTresholdInMinutes() * msecsPerMinute)
    {
        accumulatedWorkMs += workTimer.elapsed();
        working = false;
    }

    const QTime uptime = QTime(0, 0).addMSecs(totalElapsedMs());
    ui->uptimeTimeEdit->setTime(uptime);

    const QTime worktime = QTime(0, 0).addMSecs(workElapsedMs());
    ui->workingTimeTimeEdit->setTime(worktime);

    emit updated(totalElapsedMs(), workElapsedMs());
}

qint64 WorkAwareStopwatch::totalElapsedMs() const
{
    return totalTimer.elapsed();
}

qint64 WorkAwareStopwatch::workElapsedMs() const
{
    return working ? accumulatedWorkMs + workTimer.elapsed() : accumulatedWorkMs;
}

int WorkAwareStopwatch::idleTresholdInMinutes() const
{
    return ui->idleTimeTresholdSpinBox->value();
}
