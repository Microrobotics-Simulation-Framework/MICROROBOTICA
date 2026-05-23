#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <QObject>

#include "core/component_meta.h"
#include "core/physics_process.h"
#include "core/compute_backend.h"
#include "core/physics_config.h"
#include "core/result_frame.h"
#include "core/audit_logger.h"
#include "core/threadsafe_queue.h"
#include "scene/scene_manager.h"

namespace microbotica::simulation {

/// Controls the simulation lifecycle: launch, poll, stop, crash recovery.
///
/// MBCA-COMP-020
///
/// Architecture: std::async worker calls blocking receiveResult() in a loop,
/// pushes frames to a ThreadSafeQueue. requestNextFrame() polls non-blockingly
/// from the Qt main thread (driven by QTimer in TimelinePanel).
class SimulationController : public QObject {
    Q_OBJECT

public:
    explicit SimulationController(scene::SceneManager& sceneMgr,
                                   core::AuditLogger& logger,
                                   QObject* parent = nullptr);
    explicit SimulationController(scene::SceneManager& sceneMgr,
                                   QObject* parent = nullptr);
    ~SimulationController() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-020",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Simulation lifecycle controller with async polling architecture.",
            .preconditions     = {"ComputeBackend has been set via setComputeBackend()"},
            .postconditions    = {"After launchPhysics(), async worker is running",
                                  "After stop(), worker is joined and queue is drained"},
            .invariants        = {"requestNextFrame() never blocks the Qt event loop"},
            .design_rationale  = "Decouples blocking PhysicsProcess::receiveResult() from "
                                 "the Qt event loop via std::async + ThreadSafeQueue.",
            .assumptions       = {"requestNextFrame() is called from the Qt main thread",
                                  "Only one simulation runs at a time"},
            .limitations       = {"No timeout on receiveResult() — see MBCA-ANO-004",
                                  "Crash recovery clears results but does not auto-restart"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Backend hang propagates to indefinite worker block — see MBCA-ANO-004.",
                "Crash during apply leaves partial results — see MBCA-ANO-003.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Set the compute backend (must be called before launchPhysics).
    void setComputeBackend(std::unique_ptr<core::ComputeBackend> backend);

    /// Launch the physics process with the given config.
    void launchPhysics(const core::PhysicsConfig& config);

    /// Poll for the next frame (non-blocking). Call from QTimer.
    /// Emits frameReady if a frame is available.
    void requestNextFrame();

    /// Stop the simulation and join the worker.
    void stop();

    /// Full teardown: stop + release backend.
    void teardown();

    /// Check if simulation is running.
    bool isRunning() const { return running_.load(); }

    /// Pause/resume the simulation. Pauses both the worker thread and
    /// the physics process so sim time genuinely freezes.
    void setPaused(bool paused);
    bool isPaused() const { return paused_.load(); }

    /// Forward updated parameters to the underlying physics process
    /// (e.g. ParameterPanel slider edits). No-op when not running.
    void sendParameters(const nlohmann::json& params);

    /// Get current sim time.
    double currentTime() const { return currentTime_.load(); }

    /// Get the physics config.
    const core::PhysicsConfig& physicsConfig() const { return config_; }

    /// Live wire-format statistics from the physics backend (frames/sec,
    /// bytes/frame, decode latency). All-zeros when no backend streams.
    core::StreamStats streamStats() const;

    /// Number of frames actually applied to the scene. Differs from
    /// StreamStats::framesReceived because requestNextFrame() drains to the
    /// latest frame — the gap is the UI-side drop count.
    int appliedFrameCount() const { return frameCount_; }

signals:
    void frameReady(const microbotica::core::ResultFrame& frame);
    void backendCrashed(const QString& reason);
    void simulationStarted();
    void simulationStopped();

private:
    void workerFunction();

    scene::SceneManager& sceneMgr_;
    core::AuditLogger* logger_ = nullptr;
    core::NullAuditLogger nullLogger_;

    std::unique_ptr<core::ComputeBackend> backend_;
    std::unique_ptr<core::PhysicsProcess> physics_;
    core::PhysicsConfig config_;

    core::ThreadSafeQueue<core::ResultFrame> frameQueue_;
    std::future<void> workerFuture_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> errorFlag_{false};
    std::atomic<double> currentTime_{0.0};
    std::string errorMessage_;
    int frameCount_ = 0;
};

} // namespace microbotica::simulation
