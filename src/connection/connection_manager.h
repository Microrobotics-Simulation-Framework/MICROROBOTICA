#pragma once

#include <optional>
#include <string>
#include <functional>
#include "core/component_meta.h"
#include "core/stability.h"
#include "connection/connection_config.h"

namespace microbotica::connection {

/// Resolves ZMQ endpoints for connecting to MIME physics backends.
///
/// MBCA-COMP-050
///
/// Handles three connection modes transparently:
/// - Local: probe localhost, auto-detect running MIME process
/// - Manual: use user-specified endpoint directly
/// - Cloud: resolve SkyPilot cluster IP via sky_resolve.py, manage SSH tunnel
///
/// All modes produce the same output: a tcp://host:port string suitable
/// for zmq_connect().
class ConnectionManager {
public:
    ConnectionManager();

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-050",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Resolves ZMQ endpoints for MIME physics backends across Local/Manual/Cloud modes.",
            .preconditions     = {"ConnectionConfig is valid for the selected mode"},
            .postconditions    = {"resolveEndpoint returns a tcp://host:port string or empty on failure",
                                  "probe returns true only if a ZMQ REP socket responds at the endpoint"},
            .invariants        = {"ZMQ context is owned by the caller (MimePhysicsProcess), not ConnectionManager"},
            .design_rationale  = "Centralizes endpoint resolution so MimePhysicsProcess only deals with "
                                 "tcp://host:port strings regardless of how the backend was reached.",
            .assumptions       = {"Local mode assumes MIME listens on localhost:5555/5556",
                                  "Cloud mode assumes sky_resolve.py is available at scripts/sky_resolve.py",
                                  "SSH tunnel for cloud mode binds to localhost ports"},
            .limitations       = {"Cloud SSH tunnel management requires QProcess (not yet implemented — Phase D)",
                                  "No automatic reconnection on preemption (Phase D)"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Cloud endpoint resolution depends on SkyPilot cluster state — stale IPs "
                "after spot preemption will cause connection failure until re-resolved.",
                "Probe timeout too short may cause false negatives on slow networks.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Resolve a ZMQ endpoint based on the connection configuration.
    /// Returns tcp://host:port suitable for zmq_connect(), or empty string on failure.
    std::string resolveEndpoint(const ConnectionConfig& config) const;

    /// Probe whether a MIME process is reachable at the given endpoint.
    /// Sends a ZMQ REQ "ping" and waits for a reply within timeout_ms.
    /// @param endpoint  tcp://host:port for the REQ/REP socket
    /// @param timeout_ms  Maximum wait time in milliseconds (default 3000)
    /// @return true if a response was received within timeout
    bool probe(const std::string& endpoint, int timeout_ms = 3000) const;

    /// Auto-detect a local MIME process on the default port.
    /// @return Endpoint string if a MIME process responds, std::nullopt otherwise
    std::optional<std::string> autoDetectLocal(int timeout_ms = 500) const;

    /// Resolve a SkyPilot cluster IP by invoking scripts/sky_resolve.py.
    /// @param cluster_name  SkyPilot cluster name
    /// @param scripts_dir   Path to the scripts directory (for sky_resolve.py)
    /// @return JSON object with "ip", "ssh_port", "status" or std::nullopt on failure
    std::optional<nlohmann::json> resolveCluster(const std::string& cluster_name,
                                                  const std::string& scripts_dir) const;

private:
    std::string buildEndpoint(const std::string& host, int port) const;
};

} // namespace microbotica::connection
