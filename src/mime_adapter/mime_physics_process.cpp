#include "mime_adapter/mime_physics_process.h"
#include <spdlog/spdlog.h>

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

    running_.store(true);
    worker_ = std::thread(&MimePhysicsProcess::workerLoop, this);
    status_.store(core::ProcessStatus::Running);
    spdlog::info("MimePhysicsProcess: Connected to MIME at {} (SUB: {})",
                 req_endpoint_, sub_endpoint_);
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

            // Parse JSON frame
            try {
                std::string data(static_cast<char*>(msg.data()), msg.size());
                auto j = nlohmann::json::parse(data);
                auto frame = j.get<core::ResultFrame>();
                queue_.push(std::move(frame));
            } catch (const nlohmann::json::exception& e) {
                spdlog::warn("MimePhysicsProcess: Failed to parse frame JSON: {}", e.what());
                // Drop corrupted frame, continue receiving
            }
        }
    } catch (const zmq::error_t& e) {
        spdlog::error("MimePhysicsProcess: ZMQ error in worker: {}", e.what());
        status_.store(core::ProcessStatus::Error);
        running_.store(false);
    }
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
