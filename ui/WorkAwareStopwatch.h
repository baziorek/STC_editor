#pragma once

#include <QGroupBox>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime>

namespace Ui {
class WorkAwareStopwatch;
}

class WorkAwareStopwatch : public QGroupBox
{
    Q_OBJECT

public:
    explicit WorkAwareStopwatch(QWidget *parent = nullptr);
    ~WorkAwareStopwatch();

    void start();
    void stop();
    void reset();

    qint64 totalElapsedMs() const;
    qint64 workElapsedMs() const;

    int idleTresholdInMinutes() const;

public slots:
    void notifyWorkActivity();

signals:
    void updated(qint64 totalMs, qint64 workMs);

private slots:
    void onUpdate();

private:
    QElapsedTimer totalTimer;
    QElapsedTimer workTimer;

    QTimer checkIdleTimer;
    QDateTime lastWorkTime;

    QDateTime appStartTime;

    bool working = false;
    qint64 accumulatedWorkMs = 0;

private:
    Ui::WorkAwareStopwatch *ui;
};
