#pragma once

#include <QDockWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

#include "core/stability.h"

namespace microbotica::simulation {
class SimulationController;
} // namespace microbotica::simulation

namespace microbotica::panels {

/// Dock panel with play/pause/stop controls and sim time display.
///
/// Contains a 60 Hz QTimer that calls SimulationController::requestNextFrame()
/// to poll the frame queue from the Qt main thread.
class TimelinePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit TimelinePanel(simulation::SimulationController& simCtrl,
                           QWidget* parent = nullptr);
    ~TimelinePanel() override = default;

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onTimerTick();
    void onSimulationStarted();
    void onSimulationStopped();

private:
    simulation::SimulationController& simCtrl_;

    QPushButton* playBtn_ = nullptr;
    QPushButton* pauseBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QTimer* pollTimer_ = nullptr;
};

} // namespace microbotica::panels
