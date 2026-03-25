#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace microbotica::connection {

/// Connection mode for reaching a MIME physics backend.
enum class ConnectionMode {
    Local,   ///< Auto-detect or spawn on localhost
    Manual,  ///< User-specified endpoint
    Cloud    ///< SkyPilot cluster via SSH tunnel
};

NLOHMANN_JSON_SERIALIZE_ENUM(ConnectionMode, {
    {ConnectionMode::Local,  "local"},
    {ConnectionMode::Manual, "manual"},
    {ConnectionMode::Cloud,  "cloud"},
})

/// Configuration for connecting to a MIME physics backend.
struct ConnectionConfig {
    ConnectionMode mode = ConnectionMode::Local;
    std::string endpoint;           ///< For Manual: "tcp://192.168.1.5:5556"
    std::string skypilot_cluster;   ///< For Cloud: cluster name from sky status
    int zmq_pub_port = 5556;       ///< ZMQ PUB/SUB port (frame stream)
    int zmq_req_port = 5555;       ///< ZMQ REQ/REP port (commands)
};

inline void to_json(nlohmann::json& j, const ConnectionConfig& c) {
    j = nlohmann::json{
        {"mode", c.mode},
        {"endpoint", c.endpoint},
        {"skypilot_cluster", c.skypilot_cluster},
        {"zmq_pub_port", c.zmq_pub_port},
        {"zmq_req_port", c.zmq_req_port}
    };
}

inline void from_json(const nlohmann::json& j, ConnectionConfig& c) {
    j.at("mode").get_to(c.mode);
    if (j.contains("endpoint")) j.at("endpoint").get_to(c.endpoint);
    if (j.contains("skypilot_cluster")) j.at("skypilot_cluster").get_to(c.skypilot_cluster);
    if (j.contains("zmq_pub_port")) j.at("zmq_pub_port").get_to(c.zmq_pub_port);
    if (j.contains("zmq_req_port")) j.at("zmq_req_port").get_to(c.zmq_req_port);
}

} // namespace microbotica::connection
