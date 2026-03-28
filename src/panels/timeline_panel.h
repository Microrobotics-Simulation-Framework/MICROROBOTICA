#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QSlider>
#include <QTimer>

#include "core/stability.h"

namespace microbotica::simulation {
class SimulationController;
}

namespace microbotica::panels {

/// Timeline dock panel — simulation time display, frame polling, and
/// animation playback with scrubbing.
///
/// Two mutually exclusive modes:
/// - **Simulation mode** (default): 60 Hz QTimer polls requestNextFrame()
/// - **Playback mode**: QTimer drives time code advancement through a
///   pre-recorded USD animation. QSlider enables scrubbing.
class TimelinePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit TimelinePanel(simulation::SimulationController& simCtrl,
                           QWidget* parent = nullptr);
    ~TimelinePanel() override = default;

    // --- Simulation mode ---

    void pausePolling();
    void resumePolling();

    // --- Playback mode ---

    /// Switch to animation playback mode.
    /// Shows slider, hides sim-mode widgets.
    void enterPlaybackMode(double startTime, double endTime, double fps);

    /// Switch back to simulation mode.
    void enterSimulationMode();

    /// True if currently in playback mode.
    bool isPlaybackMode() const { return playbackMode_; }

    // --- Playback controls (called by MainWindow) ---

    void playbackPlay();
    void playbackPause();
    void playbackStop();

Q_SIGNALS:
    /// Emitted when the playback time code changes (slider or timer).
    void timeCodeChanged(double timeCode);

private Q_SLOTS:
    void onTimerTick();
    void onSimulationStarted();
    void onSimulationStopped();
    void onPlaybackTimerTick();
    void onSliderPressed();
    void onSliderReleased();
    void onSliderMoved(int value);

private:
    simulation::SimulationController& simCtrl_;

    // Shared widgets
    QLabel* timeLabel_ = nullptr;
    QLabel* fpsLabel_ = nullptr;

    // Simulation mode
    QTimer* pollTimer_ = nullptr;
    int framesSinceLastFps_ = 0;
    qint64 lastFpsTime_ = 0;

    // Playback mode
    bool playbackMode_ = false;
    QSlider* playbackSlider_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QTimer* playbackTimer_ = nullptr;
    double playbackStart_ = 0.0;
    double playbackEnd_ = 0.0;
    double playbackFps_ = 24.0;
    int currentFrame_ = 0;
    bool wasPlayingBeforeScrub_ = false;

    void updateFrameLabel();
};

} // namespace microbotica::panels
