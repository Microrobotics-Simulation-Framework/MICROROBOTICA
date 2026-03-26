#include "panels/simulation_toolbar.h"

namespace microbotica::panels {

SimulationToolbar::SimulationToolbar(simulation::SimulationController& controller,
                                       QWidget* parent)
    : QToolBar(tr("Simulation"), parent)
    , controller_(controller)
{
    setObjectName("SimulationToolbar");

    playAction_ = addAction(tr("Play"));
    playAction_->setShortcut(QKeySequence(Qt::Key_F5));
    playAction_->setToolTip(tr("Start simulation (F5)"));

    pauseAction_ = addAction(tr("Pause"));
    pauseAction_->setShortcut(QKeySequence(Qt::Key_F6));
    pauseAction_->setToolTip(tr("Pause simulation (F6)"));
    pauseAction_->setEnabled(false);

    stopAction_ = addAction(tr("Stop"));
    stopAction_->setShortcut(QKeySequence(Qt::Key_F7));
    stopAction_->setToolTip(tr("Stop simulation (F7)"));
    stopAction_->setEnabled(false);

    connect(playAction_, &QAction::triggered, this, [this]() {
        // Delegate to MainWindow's start logic — this just emits the action
        if (!controller_.isRunning()) {
            Q_EMIT playAction_->triggered();
        }
    });

    connect(pauseAction_, &QAction::triggered, this, [this]() {
        controller_.stop();
    });

    connect(stopAction_, &QAction::triggered, this, [this]() {
        controller_.stop();
    });

    connect(&controller_, &simulation::SimulationController::simulationStarted,
            this, &SimulationToolbar::onSimStarted);
    connect(&controller_, &simulation::SimulationController::simulationStopped,
            this, &SimulationToolbar::onSimStopped);

    updateActions();
}

void SimulationToolbar::onSimStarted() {
    updateActions();
}

void SimulationToolbar::onSimStopped() {
    updateActions();
}

void SimulationToolbar::updateActions() {
    bool running = controller_.isRunning();
    playAction_->setEnabled(!running);
    pauseAction_->setEnabled(running);
    stopAction_->setEnabled(running);
}

} // namespace microbotica::panels
