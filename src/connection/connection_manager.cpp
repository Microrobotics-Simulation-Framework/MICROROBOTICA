#include "connection/connection_manager.h"
#include <spdlog/spdlog.h>
#include <QProcess>

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#endif

namespace microbotica::connection {

ConnectionManager::ConnectionManager() {
    MBCA_EXPERIMENTAL_WARN("ConnectionManager");
}

std::string ConnectionManager::resolveEndpoint(const ConnectionConfig& config) const {
    switch (config.mode) {
        case ConnectionMode::Local: {
            auto detected = autoDetectLocal();
            if (detected) {
                spdlog::info("ConnectionManager: Auto-detected MIME at {}", *detected);
                return *detected;
            }
            // Fall back to default local endpoint even if probe fails —
            // caller may want to spawn MIME and retry.
            std::string fallback = buildEndpoint("localhost", config.zmq_pub_port);
            spdlog::info("ConnectionManager: No MIME detected, using default {}", fallback);
            return fallback;
        }
        case ConnectionMode::Manual: {
            if (config.endpoint.empty()) {
                spdlog::warn("ConnectionManager: Manual mode but no endpoint configured");
                return buildEndpoint("localhost", config.zmq_pub_port);
            }
            spdlog::info("ConnectionManager: Using manual endpoint {}", config.endpoint);
            return config.endpoint;
        }
        case ConnectionMode::Cloud: {
            if (config.skypilot_cluster.empty()) {
                spdlog::error("ConnectionManager: Cloud mode but no skypilot_cluster configured");
                return {};
            }
            // Cloud mode: SSH tunnel binds to localhost, so the ZMQ endpoint
            // is always localhost. The tunnel is managed externally (Phase D).
            std::string endpoint = buildEndpoint("localhost", config.zmq_pub_port);
            spdlog::info("ConnectionManager: Cloud mode — endpoint {} (via SSH tunnel to cluster '{}')",
                         endpoint, config.skypilot_cluster);
            return endpoint;
        }
    }
    return {};
}

bool ConnectionManager::probe(const std::string& endpoint, int timeout_ms) const {
#ifdef MICROBOTICA_HAS_ZMQ
    try {
        zmq::context_t ctx(1);
        zmq::socket_t sock(ctx, zmq::socket_type::req);
        sock.set(zmq::sockopt::linger, 0);
        sock.set(zmq::sockopt::rcvtimeo, timeout_ms);
        sock.set(zmq::sockopt::sndtimeo, timeout_ms);
        sock.connect(endpoint);

        zmq::message_t ping_msg(4);
        std::memcpy(ping_msg.data(), "ping", 4);
        auto send_result = sock.send(ping_msg, zmq::send_flags::none);
        if (!send_result) {
            return false;
        }

        zmq::message_t reply;
        auto recv_result = sock.recv(reply, zmq::recv_flags::none);
        return recv_result.has_value();
    } catch (const zmq::error_t& e) {
        spdlog::debug("ConnectionManager::probe failed: {}", e.what());
        return false;
    }
#else
    (void)endpoint;
    (void)timeout_ms;
    spdlog::debug("ConnectionManager::probe: ZMQ not available");
    return false;
#endif
}

std::optional<std::string> ConnectionManager::autoDetectLocal(int timeout_ms) const {
    std::string endpoint = buildEndpoint("localhost", 5555); // probe REQ port
    if (probe(endpoint, timeout_ms)) {
        return buildEndpoint("localhost", 5556); // return PUB port for subscription
    }
    return std::nullopt;
}

std::optional<nlohmann::json> ConnectionManager::resolveCluster(
    const std::string& cluster_name,
    const std::string& scripts_dir) const
{
    QProcess proc;
    proc.start("python3", {
        QString::fromStdString(scripts_dir + "/sky_resolve.py"),
        QString::fromStdString(cluster_name)
    });

    if (!proc.waitForFinished(10000)) {
        spdlog::error("ConnectionManager: sky_resolve.py timed out for cluster '{}'", cluster_name);
        return std::nullopt;
    }

    if (proc.exitCode() != 0) {
        std::string err = proc.readAllStandardError().toStdString();
        spdlog::warn("ConnectionManager: sky_resolve.py failed (exit {}): {}",
                      proc.exitCode(), err);
        return std::nullopt;
    }

    std::string output = proc.readAllStandardOutput().toStdString();
    if (output.empty()) {
        spdlog::warn("ConnectionManager: sky_resolve.py returned no output for cluster '{}'",
                      cluster_name);
        return std::nullopt;
    }

    try {
        auto result = nlohmann::json::parse(output);
        if (result.contains("status") && result["status"] == "UP") {
            return result;
        }
        spdlog::warn("ConnectionManager: Cluster '{}' status is '{}', not UP",
                      cluster_name, result.value("status", "unknown"));
        return std::nullopt;
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("ConnectionManager: Failed to parse sky_resolve.py output: {}", e.what());
        return std::nullopt;
    }
}

std::string ConnectionManager::buildEndpoint(const std::string& host, int port) const {
    return "tcp://" + host + ":" + std::to_string(port);
}

} // namespace microbotica::connection
