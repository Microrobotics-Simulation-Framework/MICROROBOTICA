#include "panels/timeline_panel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDateTime>
#include "core/profiler.h"

#include "simulation/simulation_controller.h"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace microbotica::panels {

TimelinePanel::TimelinePanel(simulation::SimulationController& simCtrl,
                               QWidget* parent)
    : QDockWidget(tr("Timeline"), parent)
    , simCtrl_(simCtrl)
{
    MBCA_EXPERIMENTAL_WARN("TimelinePanel");

    setObjectName("TimelinePanel");

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    // Top row: time label + stream benchmark readout + fps
    auto* topRow = new QHBoxLayout;
    timeLabel_ = new QLabel(tr("Sim: 0.000 s"), container);
    timeLabel_->setFont(QFont("Monospace", 11));
    streamLabel_ = new QLabel(tr(""), container);
    streamLabel_->setFont(QFont("Monospace", 9));
    streamLabel_->setToolTip(
        tr("ResultFrame stream: wire format, bytes/frame, mean decode time, "
           "and frames dropped by the UI (received minus applied)."));
    fpsLabel_ = new QLabel(tr(""), container);
    fpsLabel_->setFont(QFont("Monospace", 9));
    topRow->addWidget(timeLabel_);
    topRow->addStretch();
    topRow->addWidget(streamLabel_);
    topRow->addWidget(fpsLabel_);
    layout->addLayout(topRow);

    // Playback slider + frame label (hidden by default)
    playbackSlider_ = new QSlider(Qt::Horizontal, container);
    playbackSlider_->setVisible(false);
    connect(playbackSlider_, &QSlider::sliderPressed,
            this, &TimelinePanel::onSliderPressed);
    connect(playbackSlider_, &QSlider::sliderReleased,
            this, &TimelinePanel::onSliderReleased);
    connect(playbackSlider_, &QSlider::valueChanged,
            this, &TimelinePanel::onSliderMoved);
    layout->addWidget(playbackSlider_);

    frameLabel_ = new QLabel(container);
    frameLabel_->setFont(QFont("Monospace", 9));
    frameLabel_->setVisible(false);
    layout->addWidget(frameLabel_);

    container->setLayout(layout);
    setWidget(container);

    // Simulation poll timer (60 Hz)
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(16);
    connect(pollTimer_, &QTimer::timeout,
            this, &TimelinePanel::onTimerTick);

    // Playback timer (configured when entering playback mode)
    playbackTimer_ = new QTimer(this);
    connect(playbackTimer_, &QTimer::timeout,
            this, &TimelinePanel::onPlaybackTimerTick);

    connect(&simCtrl_, &simulation::SimulationController::simulationStarted,
            this, &TimelinePanel::onSimulationStarted);
    connect(&simCtrl_, &simulation::SimulationController::simulationStopped,
            this, &TimelinePanel::onSimulationStopped);
}

// --- Simulation mode ---

void TimelinePanel::onTimerTick()
{
    simCtrl_.requestNextFrame();
    const double t = simCtrl_.currentTime();
    timeLabel_->setText(QString("Sim: %1 s").arg(t, 0, 'f', 3));

    core::ProfileReporter::reportIfDue();

    framesSinceLastFps_++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastFpsTime_ >= 1000) {
        fpsLabel_->setText(QString("%1 fps").arg(framesSinceLastFps_));
        framesSinceLastFps_ = 0;
        lastFpsTime_ = now;

        // Stream benchmark readout (fit-up §8): wire format, on-wire frame
        // size, mean decode cost, and UI-side drop count.
        const auto stats = simCtrl_.streamStats();
        if (stats.framesReceived > 0) {
            qint64 dropped = static_cast<qint64>(stats.framesReceived)
                           - simCtrl_.appliedFrameCount();
            if (dropped < 0) dropped = 0;
            QString text = QString("%1  %2 B/f  %3 us")
                .arg(QString::fromStdString(
                         stats.compression == "none" || stats.format == "json"
                             ? stats.format
                             : stats.format + "/" + stats.compression))
                .arg(stats.lastFrameBytes)
                .arg(stats.decodeAvgUs, 0, 'f', 1);
            if (dropped > 0) {
                text += QString("  %1 drop").arg(dropped);
            }
            if (stats.decodeErrors > 0) {
                text += QString("  %1 err")
                    .arg(static_cast<qulonglong>(stats.decodeErrors));
            }
            streamLabel_->setText(text);
        }
    }
}

void TimelinePanel::pausePolling() {
    pollTimer_->stop();
    fpsLabel_->setText(tr("paused"));
}

void TimelinePanel::resumePolling() {
    lastFpsTime_ = QDateTime::currentMSecsSinceEpoch();
    framesSinceLastFps_ = 0;
    pollTimer_->start();
}

void TimelinePanel::onSimulationStarted()
{
    lastFpsTime_ = QDateTime::currentMSecsSinceEpoch();
    framesSinceLastFps_ = 0;
    streamLabel_->setText(tr(""));
    pollTimer_->start();
}

void TimelinePanel::onSimulationStopped()
{
    pollTimer_->stop();
    fpsLabel_->setText(tr(""));
    streamLabel_->setText(tr(""));
}

// --- Playback mode ---

void TimelinePanel::enterPlaybackMode(double startTime, double endTime, double fps)
{
    playbackMode_ = true;
    playbackStart_ = startTime;
    playbackEnd_ = endTime;
    currentFrame_ = static_cast<int>(startTime);

    // Clamp fps to 60 max — QTimer has ~15ms resolution on Windows,
    // so higher fps would drift noticeably.
    playbackFps_ = std::min(fps, 60.0);
    if (fps > 60.0) {
        spdlog::warn("TimelinePanel: Clamping playback fps from {} to 60 "
                     "(QTimer resolution limit)", fps);
    }

    int interval = static_cast<int>(1000.0 / playbackFps_);
    playbackTimer_->setInterval(std::max(interval, 1));

    // Configure slider.
    // QSlider uses integer steps — one position per frame.
    // This assumes integer time codes (MIME writes UsdTimeCode(sample_index)).
    // Sub-frame time codes would need a scaling factor or QDoubleSlider.
    playbackSlider_->setRange(static_cast<int>(startTime),
                               static_cast<int>(endTime));
    playbackSlider_->setValue(currentFrame_);
    playbackSlider_->setVisible(true);
    frameLabel_->setVisible(true);

    // Hide simulation-mode widgets
    fpsLabel_->setVisible(false);
    streamLabel_->setVisible(false);
    pollTimer_->stop();

    updateFrameLabel();
}

void TimelinePanel::enterSimulationMode()
{
    playbackMode_ = false;
    playbackTimer_->stop();
    playbackSlider_->setVisible(false);
    frameLabel_->setVisible(false);
    fpsLabel_->setVisible(true);
    streamLabel_->setVisible(true);
    timeLabel_->setText(tr("Sim: 0.000 s"));
}

void TimelinePanel::playbackPlay()
{
    if (!playbackMode_) return;
    playbackTimer_->start();
}

void TimelinePanel::playbackPause()
{
    if (!playbackMode_) return;
    playbackTimer_->stop();
}

void TimelinePanel::playbackStop()
{
    if (!playbackMode_) return;
    playbackTimer_->stop();
    currentFrame_ = static_cast<int>(playbackStart_);
    playbackSlider_->setValue(currentFrame_);
    Q_EMIT timeCodeChanged(static_cast<double>(currentFrame_));
    updateFrameLabel();
}

void TimelinePanel::onPlaybackTimerTick()
{
    currentFrame_++;
    if (currentFrame_ > static_cast<int>(playbackEnd_)) {
        currentFrame_ = static_cast<int>(playbackStart_); // Loop
    }
    playbackSlider_->setValue(currentFrame_);
    Q_EMIT timeCodeChanged(static_cast<double>(currentFrame_));
    updateFrameLabel();
}

void TimelinePanel::onSliderPressed()
{
    // Capture play state at the moment drag begins
    wasPlayingBeforeScrub_ = playbackTimer_->isActive();
    playbackTimer_->stop();
}

void TimelinePanel::onSliderReleased()
{
    // Resume only if play was active when drag started
    if (wasPlayingBeforeScrub_) {
        playbackTimer_->start();
    }
}

void TimelinePanel::onSliderMoved(int value)
{
    if (!playbackMode_) return;
    currentFrame_ = value;
    Q_EMIT timeCodeChanged(static_cast<double>(currentFrame_));
    updateFrameLabel();
}

void TimelinePanel::updateFrameLabel()
{
    int totalFrames = static_cast<int>(playbackEnd_ - playbackStart_) + 1;
    int displayFrame = currentFrame_ - static_cast<int>(playbackStart_);
    double timeSec = static_cast<double>(displayFrame) / playbackFps_;
    frameLabel_->setText(QString("Frame %1 / %2 (%3 s)")
                             .arg(displayFrame)
                             .arg(totalFrames)
                             .arg(timeSec, 0, 'f', 3));
    timeLabel_->setText(QString("Time: %1 s").arg(timeSec, 0, 'f', 3));
}

} // namespace microbotica::panels
