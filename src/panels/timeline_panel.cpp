#include "panels/timeline_panel.h"

#include <QHBoxLayout>
#include <QDateTime>

#include "simulation/simulation_controller.h"

namespace microbotica::panels {

TimelinePanel::TimelinePanel(simulation::SimulationController& simCtrl,
                               QWidget* parent)
    : QDockWidget(tr("Timeline"), parent)
    , simCtrl_(simCtrl)
{
    MBCA_EXPERIMENTAL_WARN("TimelinePanel");

    setObjectName("TimelinePanel");

    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);

    timeLabel_ = new QLabel(tr("Sim: 0.000 s"), container);
    timeLabel_->setFont(QFont("Monospace", 11));
    fpsLabel_ = new QLabel(tr(""), container);
    fpsLabel_->setFont(QFont("Monospace", 9));

    layout->addWidget(timeLabel_);
    layout->addStretch();
    layout->addWidget(fpsLabel_);

    container->setLayout(layout);
    setWidget(container);

    // 60 Hz poll timer for frame queue
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(16);
    connect(pollTimer_, &QTimer::timeout,
            this, &TimelinePanel::onTimerTick);

    connect(&simCtrl_, &simulation::SimulationController::simulationStarted,
            this, &TimelinePanel::onSimulationStarted);
    connect(&simCtrl_, &simulation::SimulationController::simulationStopped,
            this, &TimelinePanel::onSimulationStopped);
}

void TimelinePanel::onTimerTick()
{
    simCtrl_.requestNextFrame();
    const double t = simCtrl_.currentTime();
    timeLabel_->setText(QString("Sim: %1 s").arg(t, 0, 'f', 3));

    // Simple FPS counter
    framesSinceLastFps_++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastFpsTime_ >= 1000) {
        fpsLabel_->setText(QString("%1 fps").arg(framesSinceLastFps_));
        framesSinceLastFps_ = 0;
        lastFpsTime_ = now;
    }
}

void TimelinePanel::onSimulationStarted()
{
    lastFpsTime_ = QDateTime::currentMSecsSinceEpoch();
    framesSinceLastFps_ = 0;
    pollTimer_->start();
}

void TimelinePanel::onSimulationStopped()
{
    pollTimer_->stop();
    fpsLabel_->setText(tr(""));
}

} // namespace microbotica::panels
