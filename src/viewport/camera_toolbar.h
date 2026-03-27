#pragma once

#include <QToolBar>
#include <QAction>

namespace microbotica::simulation {
class SimulationController;
}

namespace microbotica::viewport {

class CameraController;

/// Toolbar above the viewport with simulation controls, camera snaps, and help.
///
/// Unified simulation Play/Pause/Stop + camera axis-snap buttons + fullscreen
/// toggle + help. This is the single source of truth for simulation control
/// UI — no duplicate buttons elsewhere.
class CameraToolbar : public QToolBar {
    Q_OBJECT

public:
    CameraToolbar(CameraController* controller,
                  simulation::SimulationController* simController,
                  QWidget* parent = nullptr);

    /// Wire camera controls to a (newly created) controller.
    /// Called when LocalViewport is created after scene load.
    void setCameraController(CameraController* controller);

    /// Show the viewport controls help dialog.
    static void showHelp(QWidget* parent);

Q_SIGNALS:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void fullscreenToggled();

public Q_SLOTS:
    void onSimStarted();
    void onSimStopped();
    void onSimPaused();

private:
    QAction* playAction_ = nullptr;
    QAction* pauseAction_ = nullptr;
    QAction* stopAction_ = nullptr;
    QAction* cameraSeparator_ = nullptr;  ///< Insert point for camera actions
    simulation::SimulationController* simController_ = nullptr;
    CameraController* cameraController_ = nullptr;
    QList<QAction*> cameraActions_;  ///< Removable camera-specific actions
};

} // namespace microbotica::viewport
