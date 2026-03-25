#include "mime_adapter/mime_stub_physics_process.h"
#include <cmath>
#include <chrono>

namespace microbotica::mime {

MimeStubPhysicsProcess::MimeStubPhysicsProcess() {
    MBCA_EXPERIMENTAL_WARN("MimeStubPhysicsProcess");
}

MimeStubPhysicsProcess::~MimeStubPhysicsProcess() {
    if (running_.load()) {
        stop();
    }
}

void MimeStubPhysicsProcess::launch(const core::PhysicsConfig& config) {
    if (status_.load() == core::ProcessStatus::Running) {
        throw std::runtime_error("MimeStubPhysicsProcess already running");
    }

    config_ = config;
    running_.store(true);
    status_.store(core::ProcessStatus::Launching);

    worker_ = std::thread(&MimeStubPhysicsProcess::workerLoop, this);
    status_.store(core::ProcessStatus::Running);
}

std::optional<core::ResultFrame> MimeStubPhysicsProcess::receiveResult() {
    if (!running_.load() && queue_.empty()) {
        return std::nullopt;
    }
    return queue_.wait_pop();
}

void MimeStubPhysicsProcess::sendParameters(const nlohmann::json& /*params*/) {
    // No-op for stub
}

void MimeStubPhysicsProcess::stop() {
    running_.store(false);
    if (worker_.joinable()) {
        core::ResultFrame sentinel;
        sentinel.simTime = -1.0;
        queue_.push(std::move(sentinel));
        worker_.join();
    }
    status_.store(core::ProcessStatus::Stopped);
}

core::ProcessStatus MimeStubPhysicsProcess::status() const {
    return status_.load();
}

void MimeStubPhysicsProcess::workerLoop() {
    double t = 0.0;
    const double dt = 0.5; // ~2 Hz (matching MIME sim rate, not UI frame rate)

    while (running_.load()) {
        core::ResultFrame frame;
        frame.simTime = t;

        // UMR robot: circular path in XZ plane, rotating around Y axis
        double radius = 0.003; // 3mm orbit
        frame.positions["rigid_body"] = core::Vec3f{
            radius * std::cos(t * 0.5),
            0.0,
            radius * std::sin(t * 0.5)
        };

        // Orientation: UMR tumbles around its long axis (Z rotation)
        double tumble_angle = t * 6.28; // ~1 rev/s
        frame.orientations["rigid_body"] = core::Quatf{
            std::cos(tumble_angle / 2.0),
            0.0,
            std::sin(tumble_angle / 2.0),
            0.0
        };

        // External magnetic field arrow: fixed at origin, rotates to track field direction
        frame.positions["ext_field"] = core::Vec3f{0.0, 0.0, 0.0};
        double field_angle = t * 0.5; // slow rotation matching UMR orbit
        frame.orientations["ext_field"] = core::Quatf{
            std::cos(field_angle / 2.0),
            0.0,
            std::sin(field_angle / 2.0),
            0.0
        };

        // Flow field: 4-vertex (2x2) cross-section with time-varying velocity colors
        // Colors encode velocity magnitude: blue (slow) → red (fast)
        core::MeshData flow;
        flow.vertexColors.resize(4);
        for (int i = 0; i < 4; ++i) {
            double phase = t * 0.3 + static_cast<double>(i) * 0.25;
            double v = 0.5 + 0.5 * std::sin(phase); // [0,1] velocity
            flow.vertexColors[static_cast<size_t>(i)] = core::Vec3f{
                v,        // R: high velocity → red
                0.0,
                1.0 - v   // B: low velocity → blue
            };
        }
        frame.meshes["flow_field"] = std::move(flow);

        // Scalars
        frame.scalars["step_out_frequency"] = 60.0 + 10.0 * std::sin(t * 0.1);
        frame.scalars["sim_time"] = t;

        queue_.push(std::move(frame));
        t += dt;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(dt * 1000)));
    }
}

} // namespace microbotica::mime
