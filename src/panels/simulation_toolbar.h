#pragma once

#include <QToolBar>
#include <QAction>
#include "simulation/simulation_controller.h"

namespace microbotica::panels {

/// Simulation control toolbar: play, pause, step, stop.
///
/// QActions are enabled/disabled based on SimulationController state.
/// Follows the Webots WbSimulationState pattern — toolbar state is
/// driven by signals, not polled.
class SimulationToolbar : public QToolBar {
    Q_OBJECT

public:
    SimulationToolbar(simulation::SimulationController& controller,
                      QWidget* parent = nullptr);

private Q_SLOTS:
    void onSimStarted();
    void onSimStopped();
    void updateActions();

private:
    simulation::SimulationController& controller_;
    QAction* playAction_ = nullptr;
    QAction* pauseAction_ = nullptr;
    QAction* stopAction_ = nullptr;
};

} // namespace microbotica::panels
