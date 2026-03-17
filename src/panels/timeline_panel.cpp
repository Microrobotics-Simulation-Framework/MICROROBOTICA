#include "panels/timeline_panel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

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

    playBtn_ = new QPushButton(tr("Play"), container);
    pauseBtn_ = new QPushButton(tr("Pause"), container);
    stopBtn_ = new QPushButton(tr("Stop"), container);
    timeLabel_ = new QLabel(tr("Time: 0.000 s"), container);

    pauseBtn_->setEnabled(false);
    stopBtn_->setEnabled(false);

    layout->addWidget(playBtn_);
    layout->addWidget(pauseBtn_);
    layout->addWidget(stopBtn_);
    layout->addStretch();
    layout->addWidget(timeLabel_);

    container->setLayout(layout);
    setWidget(container);

    // 60 Hz poll timer for frame queue
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(16); // ~60 Hz
    connect(pollTimer_, &QTimer::timeout,
            this, &TimelinePanel::onTimerTick);

    // Button connections
    connect(playBtn_, &QPushButton::clicked,
            this, &TimelinePanel::onPlayClicked);
    connect(pauseBtn_, &QPushButton::clicked,
            this, &TimelinePanel::onPauseClicked);
    connect(stopBtn_, &QPushButton::clicked,
            this, &TimelinePanel::onStopClicked);

    // SimulationController signals
    connect(&simCtrl_, &simulation::SimulationController::simulationStarted,
            this, &TimelinePanel::onSimulationStarted);
    connect(&simCtrl_, &simulation::SimulationController::simulationStopped,
            this, &TimelinePanel::onSimulationStopped);
}

void TimelinePanel::onPlayClicked()
{
    if (!simCtrl_.isRunning()) {
        // Simulation launch is initiated from MainWindow (which owns the backend).
        // If already launched, just resume the poll timer.
    }
    pollTimer_->start();
    playBtn_->setEnabled(false);
    pauseBtn_->setEnabled(true);
    stopBtn_->setEnabled(true);
}

void TimelinePanel::onPauseClicked()
{
    pollTimer_->stop();
    playBtn_->setEnabled(true);
    pauseBtn_->setEnabled(false);
}

void TimelinePanel::onStopClicked()
{
    pollTimer_->stop();
    simCtrl_.stop();
    playBtn_->setEnabled(true);
    pauseBtn_->setEnabled(false);
    stopBtn_->setEnabled(false);
    timeLabel_->setText(tr("Time: 0.000 s"));
}

void TimelinePanel::onTimerTick()
{
    simCtrl_.requestNextFrame();
    const double t = simCtrl_.currentTime();
    timeLabel_->setText(QString("Time: %1 s").arg(t, 0, 'f', 3));
}

void TimelinePanel::onSimulationStarted()
{
    pollTimer_->start();
    playBtn_->setEnabled(false);
    pauseBtn_->setEnabled(true);
    stopBtn_->setEnabled(true);
}

void TimelinePanel::onSimulationStopped()
{
    pollTimer_->stop();
    playBtn_->setEnabled(true);
    pauseBtn_->setEnabled(false);
    stopBtn_->setEnabled(false);
}

} // namespace microbotica::panels
