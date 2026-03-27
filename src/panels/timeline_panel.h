#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QTimer>

#include "core/stability.h"

namespace microbotica::simulation {
class SimulationController;
}

namespace microbotica::panels {

/// Timeline dock panel — simulation time display and frame polling.
///
/// Contains a 60 Hz QTimer that drives SimulationController::requestNextFrame().
/// No play/pause/stop buttons — those live in the CameraToolbar above the
/// viewport as the single source of truth for simulation control.
class TimelinePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit TimelinePanel(simulation::SimulationController& simCtrl,
                           QWidget* parent = nullptr);
    ~TimelinePanel() override = default;

    /// Pause frame polling (simulation worker keeps running).
    void pausePolling();

    /// Resume frame polling.
    void resumePolling();

private Q_SLOTS:
    void onTimerTick();
    void onSimulationStarted();
    void onSimulationStopped();

private:
    simulation::SimulationController& simCtrl_;
    QLabel* timeLabel_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    int framesSinceLastFps_ = 0;
    qint64 lastFpsTime_ = 0;
};

} // namespace microbotica::panels
