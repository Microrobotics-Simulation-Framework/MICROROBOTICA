#include "stubs/stub_physics_process.h"
#include <cmath>
#include <chrono>

namespace microbotica::stubs {

StubPhysicsProcess::StubPhysicsProcess() {
    MBCA_EXPERIMENTAL_WARN("StubPhysicsProcess");
}

StubPhysicsProcess::~StubPhysicsProcess() {
    if (running_.load()) {
        stop();
    }
}

void StubPhysicsProcess::launch(const core::PhysicsConfig& config) {
    if (status_.load() == core::ProcessStatus::Running) {
        throw std::runtime_error("StubPhysicsProcess already running");
    }

    config_ = config;
    running_.store(true);
    status_.store(core::ProcessStatus::Launching);

    worker_ = std::thread(&StubPhysicsProcess::workerLoop, this);
    status_.store(core::ProcessStatus::Running);
}

std::optional<core::ResultFrame> StubPhysicsProcess::receiveResult() {
    if (!running_.load() && queue_.empty()) {
        return std::nullopt;
    }
    return queue_.wait_pop();
}

void StubPhysicsProcess::sendParameters(const nlohmann::json& /*params*/) {
    // No-op for stub
}

void StubPhysicsProcess::stop() {
    running_.store(false);
    if (worker_.joinable()) {
        // Push a sentinel to unblock wait_pop
        core::ResultFrame sentinel;
        sentinel.simTime = -1.0;
        queue_.push(std::move(sentinel));
        worker_.join();
    }
    status_.store(core::ProcessStatus::Stopped);
}

core::ProcessStatus StubPhysicsProcess::status() const {
    return status_.load();
}

void StubPhysicsProcess::workerLoop() {
    double t = 0.0;
    const double dt = 1.0 / 60.0; // ~60 Hz

    while (running_.load()) {
        core::ResultFrame frame;
        frame.simTime = t;

        // Generate sinusoidal position for each mapped actor
        for (const auto& [actor, primPath] : config_.actorToPrimPath) {
            frame.positions[actor] = core::Vec3f{
                std::sin(t * 2.0),
                std::cos(t * 2.0),
                std::sin(t * 0.5) * 0.1
            };
        }

        // If no actors mapped, generate a default "robot" entry
        if (config_.actorToPrimPath.empty()) {
            frame.positions["robot"] = core::Vec3f{
                std::sin(t * 2.0),
                std::cos(t * 2.0),
                std::sin(t * 0.5) * 0.1
            };
        }

        frame.scalars["step_out_frequency"] = 60.0;

        queue_.push(std::move(frame));
        t += dt;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(dt * 1000)));
    }
}

} // namespace microbotica::stubs
