#include "simulation/simulation_controller.h"
#include "core/stability.h"
#include "core/profiler.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace microbotica::simulation {

namespace {

/// Append one row to the stream-benchmark CSV in the working directory, so
/// JSON-mode and binary-mode runs can be compared after the fact. The
/// header is written once when the file is first created.
void appendBenchmarkCsv(const core::StreamStats& s, int applied) {
    namespace fs = std::filesystem;
    const fs::path path = "microbotica_stream_benchmark.csv";
    std::error_code ec;
    const bool existed = fs::exists(path, ec);

    std::ofstream out(path, std::ios::app);
    if (!out) {
        spdlog::warn("SimulationController: could not open {} for the "
                     "stream benchmark log", path.string());
        return;
    }
    if (!existed) {
        out << "timestamp,format,compression,frames_received,applied,"
               "dropped,total_bytes,avg_frame_bytes,decode_avg_us,"
               "decode_max_us,decode_errors,wire_fps\n";
    }

    const std::time_t now = std::time(nullptr);
    char ts[32] = {0};
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

    const long long dropped = std::max<long long>(
        0, static_cast<long long>(s.framesReceived) - applied);
    const double avg_frame = s.framesReceived
        ? static_cast<double>(s.bytesReceived)
              / static_cast<double>(s.framesReceived)
        : 0.0;

    out << ts << ',' << s.format << ',' << s.compression << ','
        << s.framesReceived << ',' << applied << ',' << dropped << ','
        << s.bytesReceived << ',' << avg_frame << ',' << s.decodeAvgUs << ','
        << s.decodeMaxUs << ',' << s.decodeErrors << ',' << s.receivedFps
        << '\n';

    spdlog::info("SimulationController: stream benchmark row appended to {}",
                 fs::absolute(path, ec).string());
}

} // namespace

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

    MBCA_PROFILE_SCOPE("drain_queue");

    // Single-lock drain: get the newest frame, discard stale ones.
    auto latest = frameQueue_.drain_latest();

    if (latest.has_value()) {
        if (latest->simTime < 0.0) return; // sentinel
        currentTime_.store(latest->simTime);
        frameCount_++;

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

    // Stream benchmark summary (fit-up §8). The worker has joined, so the
    // backend's stats are final and stable.
    if (physics_) {
        const core::StreamStats s = physics_->streamStats();
        if (s.framesReceived > 0) {
            const long long dropped = std::max<long long>(
                0, static_cast<long long>(s.framesReceived) - frameCount_);
            const double avg_frame =
                static_cast<double>(s.bytesReceived)
                / static_cast<double>(s.framesReceived);
            spdlog::info(
                "SimulationController: stream benchmark — format={} "
                "compression={} received={} applied={} dropped={} "
                "total_bytes={} avg_frame={:.0f}B decode_avg={:.1f}us "
                "decode_max={:.1f}us decode_errors={} wire_fps={:.1f}",
                s.format, s.compression, s.framesReceived, frameCount_,
                dropped, s.bytesReceived, avg_frame, s.decodeAvgUs,
                s.decodeMaxUs, s.decodeErrors, s.receivedFps);
            appendBenchmarkCsv(s, frameCount_);
        }
    }

    simulationStopped();
}

core::StreamStats SimulationController::streamStats() const {
    return physics_ ? physics_->streamStats() : core::StreamStats{};
}

void SimulationController::setPaused(bool paused) {
    paused_.store(paused);
    if (physics_) {
        physics_->setPaused(paused);
    }
}

void SimulationController::sendParameters(const nlohmann::json& params) {
    if (physics_ && running_.load()) {
        physics_->sendParameters(params);
    }
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
