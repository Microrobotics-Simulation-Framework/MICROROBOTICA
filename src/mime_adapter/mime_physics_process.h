#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include "core/physics_process.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "core/stream_stats.h"
#include "core/threadsafe_queue.h"
#include "connection/connection_config.h"
#include "mime_adapter/binary_frame_decoder.h"

namespace microbotica::mime {

/// Physics process that receives ResultFrames from a MIME backend over ZMQ.
///
/// MBCA-IMPL-010
///
/// Uses ZMQ REQ/REP for commands (launch, stop, sendParameters) and
/// ZMQ SUB for receiving ResultFrames. The endpoint is provided by
/// ConnectionManager.
///
/// The 5556 stream carries one of two ResultFrame wire formats (MIME v0.2
/// fit-up §8): JSON (default, unchanged) or compact binary. On connect the
/// process performs a `stream_info` handshake on the REP socket; the runner
/// declares the active format and, for binary, a decode schema. JSON is the
/// fallback if the handshake is unavailable (older runner).
///
/// IPC Protocol:
///   REQ/REP (tcp://host:5555):
///     "launch"                → {"status":"ok"} | "error:..."
///     "stop"                  → {"status":"ok"}
///     {"command":"params",...}→ {"status":"ok"}
///     {"command":"stream_info"} → {"status":"ok","format":...,"schema":...}
///     "ping"                  → "pong"
///   PUB/SUB (tcp://host:5556):
///     topic "" → ResultFrame (JSON text, or binary frame per the schema);
///                plus a one-time {"type":"params",...} JSON notice at startup
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
    void setPaused(bool paused) override;
    void stop() override;
    core::ProcessStatus status() const override;

    /// Snapshot of the live stream statistics (thread-safe). Reflects the
    /// negotiated format and the per-frame wire size / decode cost measured
    /// in the SUB worker.
    core::StreamStats streamStats() const override;

    /// Negotiated 5556 wire format (MIME v0.2 fit-up §8).
    enum class StreamFormat { Json, Binary };

private:
    void workerLoop();
    std::string sendCommand(const std::string& command);

    /// Perform the connect-time `stream_info` handshake on the REP socket.
    /// Sets stream_format_ (and binary_schema_ if the runner supplies it).
    /// Falls back to JSON on any error or older-runner reply. Called from
    /// launch() before the worker thread starts.
    void negotiateStreamFormat();

    /// Re-query `stream_info` until the runner reports a binary decode
    /// schema. The runner builds its encoder lazily, so the schema is null
    /// until the first binary frame is published. Returns false if the
    /// schema never arrives or the process is stopped. Called from the
    /// worker thread.
    bool acquireBinarySchema();

    /// Decode a JSON text message off the SUB socket. Returns the decoded
    /// ResultFrame, or std::nullopt for the one-time startup
    /// {"type":"params",...} notice. Throws nlohmann::json::parse_error on
    /// malformed JSON (the caller uses that to fall back to binary decoding).
    std::optional<core::ResultFrame> decodeJsonMessage(const std::uint8_t* data,
                                                       std::size_t size);

    std::string req_endpoint_;
    std::string sub_endpoint_;

    core::PhysicsConfig config_;
    std::atomic<core::ProcessStatus> status_{core::ProcessStatus::Idle};
    std::atomic<bool> running_{false};
    std::thread worker_;
    core::ThreadSafeQueue<core::ResultFrame> queue_;

    // Stream format state. Written by launch()/negotiateStreamFormat()
    // before the worker thread is created (thread creation publishes the
    // write); thereafter touched only by the worker thread.
    StreamFormat stream_format_ = StreamFormat::Json;
    BinaryStreamSchema binary_schema_;

    // Live stream statistics. Updated by the worker thread, read via
    // streamStats() from the UI thread — guarded by stats_mutex_.
    mutable std::mutex stats_mutex_;
    core::StreamStats stats_;
};

} // namespace microbotica::mime
