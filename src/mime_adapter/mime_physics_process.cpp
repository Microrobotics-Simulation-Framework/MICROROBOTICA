#include "mime_adapter/mime_physics_process.h"
#include "mime_adapter/binary_frame_decoder.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#endif

namespace microbotica::mime {

MimePhysicsProcess::MimePhysicsProcess(std::string req_endpoint,
                                         std::string sub_endpoint)
    : req_endpoint_(std::move(req_endpoint))
    , sub_endpoint_(std::move(sub_endpoint))
{
    MBCA_EXPERIMENTAL_WARN("MimePhysicsProcess");
}

MimePhysicsProcess::~MimePhysicsProcess() {
    if (running_.load()) {
        stop();
    }
}

void MimePhysicsProcess::launch(const core::PhysicsConfig& config) {
    if (status_.load() == core::ProcessStatus::Running) {
        throw std::runtime_error("MimePhysicsProcess already running");
    }

    config_ = config;
    status_.store(core::ProcessStatus::Launching);

#ifdef MICROBOTICA_HAS_ZMQ
    // Send launch command to MIME backend
    try {
        std::string reply = sendCommand("launch");
        if (reply.substr(0, 5) == "error") {
            spdlog::error("MimePhysicsProcess: MIME backend rejected launch: {}", reply);
            status_.store(core::ProcessStatus::Error);
            return;
        }
    } catch (const std::exception& e) {
        spdlog::error("MimePhysicsProcess: Failed to send launch command: {}", e.what());
        status_.store(core::ProcessStatus::Error);
        return;
    }

    // Connect-time handshake (MIME v0.2 fit-up §8): ask the runner which
    // wire format it streams on 5556. Runs before the worker thread is
    // created so the negotiated format/schema are safely published to it.
    negotiateStreamFormat();

    running_.store(true);
    worker_ = std::thread(&MimePhysicsProcess::workerLoop, this);
    status_.store(core::ProcessStatus::Running);
    spdlog::info("MimePhysicsProcess: Connected to MIME at {} (SUB: {}, stream: {})",
                 req_endpoint_, sub_endpoint_,
                 stream_format_ == StreamFormat::Binary ? "binary" : "json");
#else
    spdlog::error("MimePhysicsProcess: ZMQ not available — cannot connect to MIME backend");
    status_.store(core::ProcessStatus::Error);
#endif
}

std::optional<core::ResultFrame> MimePhysicsProcess::receiveResult() {
    if (!running_.load() && queue_.empty()) {
        return std::nullopt;
    }
    return queue_.wait_pop();
}

void MimePhysicsProcess::sendParameters(const nlohmann::json& params) {
#ifdef MICROBOTICA_HAS_ZMQ
    try {
        // MIME runner expects {"command":"params","data":{...}}.
        nlohmann::json msg = {{"command", "params"}, {"data", params}};
        std::string reply = sendCommand(msg.dump());
        if (reply.find("error") != std::string::npos) {
            spdlog::warn("MimePhysicsProcess: Parameter update rejected: {}", reply);
        }
    } catch (const std::exception& e) {
        spdlog::warn("MimePhysicsProcess: Failed to send parameters: {}", e.what());
    }
#else
    (void)params;
#endif
}

void MimePhysicsProcess::setPaused(bool paused) {
#ifdef MICROBOTICA_HAS_ZMQ
    try {
        nlohmann::json msg = {{"command", paused ? "pause" : "resume"}};
        sendCommand(msg.dump());
    } catch (const std::exception& e) {
        spdlog::warn("MimePhysicsProcess: setPaused({}) failed: {}",
                      paused, e.what());
    }
#else
    (void)paused;
#endif
}

void MimePhysicsProcess::stop() {
#ifdef MICROBOTICA_HAS_ZMQ
    try {
        sendCommand("stop");
    } catch (const std::exception& e) {
        spdlog::debug("MimePhysicsProcess: Stop command failed (backend may already be down): {}",
                       e.what());
    }
#endif

    running_.store(false);
    if (worker_.joinable()) {
        // Push sentinel to unblock wait_pop
        core::ResultFrame sentinel;
        sentinel.simTime = -1.0;
        queue_.push(std::move(sentinel));
        worker_.join();
    }
    status_.store(core::ProcessStatus::Stopped);
    spdlog::info("MimePhysicsProcess: Stopped");
}

core::ProcessStatus MimePhysicsProcess::status() const {
    return status_.load();
}

core::StreamStats MimePhysicsProcess::streamStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void MimePhysicsProcess::workerLoop() {
#ifdef MICROBOTICA_HAS_ZMQ
    try {
        zmq::context_t ctx(1);
        zmq::socket_t sub(ctx, zmq::socket_type::sub);
        sub.set(zmq::sockopt::rcvtimeo, 10000); // 10s heartbeat timeout
        sub.set(zmq::sockopt::linger, 0);
        sub.set(zmq::sockopt::subscribe, ""); // subscribe to all topics
        sub.connect(sub_endpoint_);

        spdlog::debug("MimePhysicsProcess: SUB socket connected to {}", sub_endpoint_);

        // For a binary stream, make sure we hold a decode schema before
        // consuming frames. The runner builds its encoder lazily, so the
        // schema may not have been ready at handshake time.
        if (stream_format_ == StreamFormat::Binary && !binary_schema_.valid()) {
            if (!acquireBinarySchema()) {
                if (running_.load()) {
                    spdlog::error("MimePhysicsProcess: could not obtain binary "
                                  "stream schema — aborting receive loop");
                    status_.store(core::ProcessStatus::Error);
                }
                running_.store(false);
                return;
            }
        }

        // Publish the negotiated format/compression into the live stats.
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.format = (stream_format_ == StreamFormat::Binary)
                          ? "binary" : "json";
            stats_.compression = (stream_format_ == StreamFormat::Binary)
                               ? binary_schema_.compression : "none";
        }

        bool have_prev_frame = false;
        std::chrono::steady_clock::time_point prev_frame_tp;

        while (running_.load()) {
            zmq::message_t msg;
            auto result = sub.recv(msg, zmq::recv_flags::none);

            if (!result) {
                // Timeout — no frame received in 10s
                spdlog::warn("MimePhysicsProcess: No frame received in 10s — connection may be lost");
                status_.store(core::ProcessStatus::Error);
                running_.store(false);
                break;
            }

            const auto* bytes = static_cast<const std::uint8_t*>(msg.data());
            const std::size_t size = msg.size();
            if (size == 0) {
                continue;
            }

            // Discriminate per message: a JSON message always starts with
            // '{' (0x7B); a binary frame starts with an 8-byte float64
            // sim_time. The runner also publishes a one-time JSON
            // {"type":"params",...} notice regardless of stream format.
            const auto decode_start = std::chrono::steady_clock::now();
            std::optional<core::ResultFrame> frame;
            bool is_params = false;
            bool failed = false;

            try {
                bool decoded_as_json = false;
                if (looksLikeJsonMessage(bytes, size)) {
                    try {
                        auto parsed = decodeJsonMessage(bytes, size);
                        decoded_as_json = true;
                        if (parsed.has_value()) {
                            frame = std::move(parsed);
                        } else {
                            is_params = true;
                        }
                    } catch (const nlohmann::json::parse_error&) {
                        // Not valid JSON. In binary mode this is a frame
                        // whose float64 sim_time happened to begin with
                        // 0x7B — fall through to the binary decoder.
                        if (stream_format_ != StreamFormat::Binary) {
                            throw; // genuinely corrupt JSON in JSON mode
                        }
                    }
                }
                if (!decoded_as_json) {
                    if (stream_format_ == StreamFormat::Binary) {
                        frame = decodeBinaryFrame(bytes, size, binary_schema_);
                    } else {
                        throw std::runtime_error(
                            "non-JSON message in JSON stream mode");
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("MimePhysicsProcess: Failed to decode frame: {}",
                             e.what());
                failed = true;
            }

            const double decode_us =
                std::chrono::duration<double, std::micro>(
                    std::chrono::steady_clock::now() - decode_start).count();

            if (failed) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.decodeErrors++;
                continue;
            }
            if (is_params) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.paramsMessages++;
                continue;
            }
            if (!frame.has_value()) {
                continue; // defensive — should not happen
            }

            // Frame-rate EWMA from inter-frame wall time.
            const auto now_tp = std::chrono::steady_clock::now();
            double inst_fps = 0.0;
            if (have_prev_frame) {
                const double dt =
                    std::chrono::duration<double>(now_tp - prev_frame_tp).count();
                if (dt > 0.0) {
                    inst_fps = 1.0 / dt;
                }
            }
            prev_frame_tp = now_tp;
            have_prev_frame = true;

            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.framesReceived++;
                stats_.bytesReceived += size;
                stats_.lastFrameBytes = static_cast<std::uint32_t>(size);
                stats_.decodeAvgUs +=
                    (decode_us - stats_.decodeAvgUs)
                    / static_cast<double>(stats_.framesReceived);
                if (decode_us > stats_.decodeMaxUs) {
                    stats_.decodeMaxUs = decode_us;
                }
                if (inst_fps > 0.0) {
                    stats_.receivedFps = (stats_.receivedFps <= 0.0)
                        ? inst_fps
                        : 0.9 * stats_.receivedFps + 0.1 * inst_fps;
                }
            }

            queue_.push(std::move(*frame));
        }
    } catch (const zmq::error_t& e) {
        spdlog::error("MimePhysicsProcess: ZMQ error in worker: {}", e.what());
        status_.store(core::ProcessStatus::Error);
        running_.store(false);
    }
#endif
}

std::optional<core::ResultFrame> MimePhysicsProcess::decodeJsonMessage(
    const std::uint8_t* data, std::size_t size) {
    const std::string text(reinterpret_cast<const char*>(data), size);
    auto j = nlohmann::json::parse(text); // throws json::parse_error on bad JSON

    // The runner emits a one-time {"type":"params",...} notice on the PUB
    // socket at startup, in both stream formats. Recognise and skip it —
    // it is not a ResultFrame.
    if (j.is_object() && j.value("type", std::string()) == "params") {
        spdlog::debug("MimePhysicsProcess: received startup params notice");
        return std::nullopt;
    }

    return j.get<core::ResultFrame>();
}

void MimePhysicsProcess::negotiateStreamFormat() {
#ifdef MICROBOTICA_HAS_ZMQ
    // Default to JSON; only a clean "binary" reply flips us over.
    stream_format_ = StreamFormat::Json;
    try {
        const nlohmann::json req = {{"command", "stream_info"}};
        const std::string reply = sendCommand(req.dump());
        const auto j = nlohmann::json::parse(reply);
        const std::string status = j.value("status", std::string());
        const std::string format = j.value("format", std::string());

        if (status == "ok" && format == "binary") {
            stream_format_ = StreamFormat::Binary;
            spdlog::info("MimePhysicsProcess: runner declared binary stream format");
            const auto schema_it = j.find("schema");
            if (schema_it != j.end() && schema_it->is_object()) {
                try {
                    binary_schema_ = parseBinarySchema(*schema_it);
                    spdlog::info("MimePhysicsProcess: binary schema received "
                                 "({} floats/frame, compression={})",
                                 binary_schema_.total_floats,
                                 binary_schema_.compression);
                } catch (const std::exception& e) {
                    spdlog::warn("MimePhysicsProcess: bad binary schema in "
                                 "stream_info reply ({}) — will re-query",
                                 e.what());
                }
            } else {
                spdlog::info("MimePhysicsProcess: binary schema not ready yet "
                             "(encoder built lazily) — will re-query");
            }
        } else if (status == "ok" && format == "json") {
            spdlog::info("MimePhysicsProcess: runner declared JSON stream format");
        } else {
            // Older runner (replies "unknown_command") or unexpected shape.
            spdlog::info("MimePhysicsProcess: stream_info unavailable "
                         "(reply: {}) — defaulting to JSON", reply);
        }
    } catch (const std::exception& e) {
        spdlog::warn("MimePhysicsProcess: stream_info handshake failed ({}) "
                     "— defaulting to JSON", e.what());
    }
#endif
}

bool MimePhysicsProcess::acquireBinarySchema() {
#ifdef MICROBOTICA_HAS_ZMQ
    // The runner's BinaryStateEncoder is built from the first published
    // frame, so stream_info returns schema:null until then. Re-query until
    // the schema appears (or the process is told to stop).
    constexpr int kMaxAttempts = 100; // ~10s at 100ms spacing
    for (int attempt = 1; attempt <= kMaxAttempts && running_.load(); ++attempt) {
        try {
            const nlohmann::json req = {{"command", "stream_info"}};
            const std::string reply = sendCommand(req.dump());
            const auto j = nlohmann::json::parse(reply);
            const auto schema_it = j.find("schema");
            if (j.value("format", std::string()) == "binary"
                && schema_it != j.end() && schema_it->is_object()) {
                binary_schema_ = parseBinarySchema(*schema_it);
                if (binary_schema_.valid()) {
                    spdlog::info("MimePhysicsProcess: binary schema acquired "
                                 "after {} attempt(s) ({} floats/frame, "
                                 "compression={})",
                                 attempt, binary_schema_.total_floats,
                                 binary_schema_.compression);
                    return true;
                }
            }
        } catch (const std::exception& e) {
            spdlog::debug("MimePhysicsProcess: stream_info re-query {} "
                          "failed: {}", attempt, e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
#else
    return false;
#endif
}

std::string MimePhysicsProcess::sendCommand(const std::string& command) {
#ifdef MICROBOTICA_HAS_ZMQ
    // Up to 3 attempts. Each attempt uses a fresh REQ socket so a
    // dropped reply leaves no half-finished state on this side.
    std::string last_error;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        zmq::context_t ctx(1);
        zmq::socket_t req(ctx, zmq::socket_type::req);
        req.set(zmq::sockopt::linger, 0);
        req.set(zmq::sockopt::rcvtimeo, 5000);
        req.set(zmq::sockopt::sndtimeo, 5000);
        try {
            req.connect(req_endpoint_);
            zmq::message_t msg(command.size());
            std::memcpy(msg.data(), command.data(), command.size());
            auto sent = req.send(msg, zmq::send_flags::none);
            if (!sent) {
                last_error = "send timeout";
            } else {
                zmq::message_t reply;
                auto result = req.recv(reply, zmq::recv_flags::none);
                if (result) {
                    return std::string(
                        static_cast<char*>(reply.data()), reply.size());
                }
                last_error = "recv timeout";
            }
        } catch (const zmq::error_t& e) {
            last_error = e.what();
        }
        spdlog::warn("MimePhysicsProcess::sendCommand attempt {} failed: "
                      "{} — retrying", attempt, last_error);
    }
    throw std::runtime_error("Command timeout: no reply from MIME backend ("
                              + last_error + ")");
#else
    (void)command;
    return "error:zmq_not_available";
#endif
}

} // namespace microbotica::mime
