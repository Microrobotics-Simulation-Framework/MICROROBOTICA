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
    simulation::SimulationController* simController_ = nullptr;
};

} // namespace microbotica::viewport
