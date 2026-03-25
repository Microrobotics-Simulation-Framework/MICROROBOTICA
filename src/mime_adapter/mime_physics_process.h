#pragma once

#include <atomic>
#include <thread>
#include <string>
#include "core/physics_process.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "core/threadsafe_queue.h"
#include "connection/connection_config.h"

namespace microbotica::mime {

/// Physics process that receives ResultFrames from a MIME backend over ZMQ.
///
/// MBCA-IMPL-010
///
/// Uses ZMQ REQ/REP for commands (launch, stop, sendParameters) and
/// ZMQ SUB for receiving ResultFrame JSON. The endpoint is provided
/// by ConnectionManager.
///
/// IPC Protocol:
///   REQ/REP (tcp://host:5555):
///     "launch"            → "ok" | "error:..."
///     "stop"              → "ok"
///     {"params": {...}}   → "ok"
///     "ping"              → "pong"
///   PUB/SUB (tcp://host:5556):
///     topic "" → ResultFrame JSON
class MimePhysicsProcess : public core::PhysicsProcess {
public:
    /// Construct with resolved ZMQ endpoints.
    /// @param req_endpoint  tcp://host:port for REQ/REP commands
    /// @param sub_endpoint  tcp://host:port for SUB frame stream
    MimePhysicsProcess(std::string req_endpoint, std::string sub_endpoint);
    ~MimePhysicsProcess() override;

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-010",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Physics process receiving ResultFrames from MIME over ZMQ.",
            .preconditions     = {"MIME backend is running and listening on the configured endpoints"},
            .postconditions    = {"ResultFrames are received as JSON and deserialized",
                                  "Connection loss triggers backendCrashed status after 10s timeout"},
            .invariants        = {"ZMQ context is owned by this instance"},
            .design_rationale  = "Subprocess + ZMQ avoids JAX/CUDA context conflicts with the UI's "
                                 "OpenGL context and allows clean crash recovery.",
            .assumptions       = {"MIME publishes ResultFrame JSON on the SUB socket",
                                  "MIME responds to REQ commands with JSON or simple strings",
                                  "Network latency is low enough for 2+ fps frame delivery"},
            .limitations       = {"10s heartbeat timeout may cause false disconnects on slow networks",
                                  "No automatic reconnection — caller must re-create on connection loss",
                                  "receiveResult blocks indefinitely if ZMQ recv timeout is disabled"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Connection loss between MIME and MICROBOTICA causes data gap — the "
                "last displayed frame may be stale. See MBCA-ANO-003.",
                "JSON parse failure on a corrupted frame drops that frame silently.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    void launch(const core::PhysicsConfig& config) override;
    std::optional<core::ResultFrame> receiveResult() override;
    void sendParameters(const nlohmann::json& params) override;
    void stop() override;
    core::ProcessStatus status() const override;

private:
    void workerLoop();
    std::string sendCommand(const std::string& command);

    std::string req_endpoint_;
    std::string sub_endpoint_;

    core::PhysicsConfig config_;
    std::atomic<core::ProcessStatus> status_{core::ProcessStatus::Idle};
    std::atomic<bool> running_{false};
    std::thread worker_;
    core::ThreadSafeQueue<core::ResultFrame> queue_;
};

} // namespace microbotica::mime
