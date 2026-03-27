#include "simulation/simulation_controller.h"
#include "core/stability.h"
#include <spdlog/spdlog.h>

namespace microbotica::simulation {

SimulationController::SimulationController(scene::SceneManager& sceneMgr,
                                             core::AuditLogger& logger,
                                             QObject* parent)
    : QObject(parent), sceneMgr_(sceneMgr), logger_(&logger)
{
    MBCA_EXPERIMENTAL_WARN("SimulationController");
}

SimulationController::SimulationController(scene::SceneManager& sceneMgr,
                                             QObject* parent)
    : QObject(parent), sceneMgr_(sceneMgr), logger_(&nullLogger_)
{
    MBCA_EXPERIMENTAL_WARN("SimulationController");
}

SimulationController::~SimulationController() {
    teardown();
}

void SimulationController::setComputeBackend(
    std::unique_ptr<core::ComputeBackend> backend)
{
    backend_ = std::move(backend);
}

void SimulationController::launchPhysics(const core::PhysicsConfig& config) {
    if (!backend_) {
        spdlog::error("SimulationController: No compute backend set");
        return;
    }
    if (running_.load()) {
        spdlog::warn("SimulationController: Already running, stopping first");
        stop();
    }

    config_ = config;
    physics_ = backend_->createPhysicsProcess();
    physics_->launch(config);

    running_.store(true);
    errorFlag_.store(false);
    frameCount_ = 0;
    currentTime_.store(0.0);

    // Launch async worker that reads from physics and pushes to queue
    workerFuture_ = std::async(std::launch::async,
                                &SimulationController::workerFunction, this);

    spdlog::info("SimulationController: Physics launched");
    logger_->logEvent("simulation_start", {{"backend", backend_->backendName()}});
    simulationStarted();
}

void SimulationController::requestNextFrame() {
    // Check for crash
    if (errorFlag_.load()) {
        running_.store(false);
        spdlog::error("SimulationController: Backend crashed: {}", errorMessage_);
        sceneMgr_.crashRecovery();
        backendCrashed(QString::fromStdString(errorMessage_));
        errorFlag_.store(false);
        return;
    }

    // Drain the queue to the latest frame. If multiple frames have
    // accumulated (timer jitter, slow paintGL), skip intermediate ones
    // to keep the viewport showing the most recent state.
    std::optional<core::ResultFrame> latest;
    int skipped = 0;
    while (auto frame = frameQueue_.try_pop()) {
        if (frame->simTime < 0.0) continue; // sentinel
        if (latest.has_value()) skipped++;
        latest = std::move(*frame);
    }

    if (latest.has_value()) {
        currentTime_.store(latest->simTime);
        frameCount_ += skipped + 1;

        // Apply only the latest frame to the scene
        sceneMgr_.applyResultFrame(*latest, config_);
        logger_->logResultFrame(frameCount_, latest->simTime, false);

        Q_EMIT frameReady(*latest);
    }
}

void SimulationController::stop() {
    if (!running_.load()) return;

    running_.store(false);
    if (physics_) {
        physics_->stop();
    }

    if (workerFuture_.valid()) {
        workerFuture_.wait();
    }

    spdlog::info("SimulationController: Stopped after {} frames", frameCount_);
    logger_->logEvent("simulation_stop", {{"frames", frameCount_}});
    simulationStopped();
}

void SimulationController::teardown() {
    stop();
    physics_.reset();
    backend_.reset();
}

void SimulationController::workerFunction() {
    try {
        while (running_.load()) {
            // When paused, sleep instead of producing frames.
            // This freezes sim time because the physics process
            // isn't called and no frames are generated.
            if (paused_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            auto frame = physics_->receiveResult();
            if (!frame.has_value()) {
                running_.store(false);
                break;
            }
            frameQueue_.push(std::move(*frame));
        }
    } catch (const std::exception& e) {
        errorMessage_ = e.what();
        errorFlag_.store(true);
    }
}

} // namespace microbotica::simulation
