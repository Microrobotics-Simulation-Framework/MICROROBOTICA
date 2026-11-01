#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>

#include "connection/connection_config.h"
#include "connection/connection_manager.h"

using namespace microbotica::connection;
using namespace microbotica::core;

// ── ConnectionConfig tests ───────────────────────────────────────────────────

TEST_CASE("ConnectionConfig: defaults", "[unit][connection]") {
    ConnectionConfig config;
    REQUIRE(config.mode == ConnectionMode::Local);
    REQUIRE(config.endpoint.empty());
    REQUIRE(config.skypilot_cluster.empty());
    REQUIRE(config.zmq_pub_port == 5556);
    REQUIRE(config.zmq_req_port == 5555);
}

TEST_CASE("ConnectionConfig: JSON roundtrip", "[unit][connection]") {
    ConnectionConfig original;
    original.mode = ConnectionMode::Manual;
    original.endpoint = "tcp://192.168.1.5:5556";
    original.zmq_pub_port = 6000;
    original.zmq_req_port = 6001;

    nlohmann::json j = original;
    auto restored = j.get<ConnectionConfig>();

    REQUIRE(restored.mode == ConnectionMode::Manual);
    REQUIRE(restored.endpoint == "tcp://192.168.1.5:5556");
    REQUIRE(restored.zmq_pub_port == 6000);
    REQUIRE(restored.zmq_req_port == 6001);
}

TEST_CASE("ConnectionConfig: JSON mode serialization", "[unit][connection]") {
    ConnectionConfig config;

    config.mode = ConnectionMode::Local;
    nlohmann::json j1 = config;
    REQUIRE(j1["mode"] == "local");

    config.mode = ConnectionMode::Manual;
    nlohmann::json j2 = config;
    REQUIRE(j2["mode"] == "manual");

    config.mode = ConnectionMode::Cloud;
    nlohmann::json j3 = config;
    REQUIRE(j3["mode"] == "cloud");
}

// ── ConnectionManager tests ──────────────────────────────────────────────────

TEST_CASE("ConnectionManager: meta is valid", "[unit][connection]") {
    const auto& m = ConnectionManager::interfaceMeta();
    REQUIRE(m.component_id == "MBCA-COMP-050");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

TEST_CASE("ConnectionManager: resolveEndpoint local mode", "[unit][connection]") {
    ConnectionManager mgr;
    ConnectionConfig config;
    config.mode = ConnectionMode::Local;
    config.zmq_pub_port = 5556;

    // No MIME running, so auto-detect fails, but should return default endpoint
    std::string endpoint = mgr.resolveEndpoint(config);
    REQUIRE(endpoint == "tcp://localhost:5556");
}

TEST_CASE("ConnectionManager: resolveEndpoint manual mode", "[unit][connection]") {
    ConnectionManager mgr;
    ConnectionConfig config;
    config.mode = ConnectionMode::Manual;
    config.endpoint = "tcp://10.0.0.5:7777";

    std::string endpoint = mgr.resolveEndpoint(config);
    REQUIRE(endpoint == "tcp://10.0.0.5:7777");
}

TEST_CASE("ConnectionManager: resolveEndpoint manual mode empty", "[unit][connection]") {
    ConnectionManager mgr;
    ConnectionConfig config;
    config.mode = ConnectionMode::Manual;
    config.zmq_pub_port = 5556;
    // endpoint left empty

    std::string endpoint = mgr.resolveEndpoint(config);
    REQUIRE(endpoint == "tcp://localhost:5556");
}

TEST_CASE("ConnectionManager: resolveEndpoint cloud mode", "[unit][connection]") {
    ConnectionManager mgr;
    ConnectionConfig config;
    config.mode = ConnectionMode::Cloud;
    config.skypilot_cluster = "my-cluster";
    config.zmq_pub_port = 5556;

    // Cloud mode returns localhost (SSH tunnel binds locally)
    std::string endpoint = mgr.resolveEndpoint(config);
    REQUIRE(endpoint == "tcp://localhost:5556");
}

TEST_CASE("ConnectionManager: resolveEndpoint cloud mode no cluster", "[unit][connection]") {
    ConnectionManager mgr;
    ConnectionConfig config;
    config.mode = ConnectionMode::Cloud;
    // skypilot_cluster left empty

    std::string endpoint = mgr.resolveEndpoint(config);
    REQUIRE(endpoint.empty());
}

TEST_CASE("ConnectionManager: probe on closed port returns false", "[unit][connection]") {
    ConnectionManager mgr;
    // Use a high port number unlikely to be in use
    bool result = mgr.probe("tcp://localhost:59999", 200);
    REQUIRE_FALSE(result);
}

TEST_CASE("ConnectionManager: autoDetectLocal returns nullopt when nothing running", "[unit][connection]") {
    ConnectionManager mgr;
    auto result = mgr.autoDetectLocal(200);
    REQUIRE_FALSE(result.has_value());
}

// ── ZMQ mock integration test ────────────────────────────────────────────────

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>

TEST_CASE("ConnectionManager: probe succeeds against mock REP", "[integration][connection]") {
    const std::string ep = "tcp://127.0.0.1:15557";

    std::atomic<bool> running{true};
    std::thread server([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 2000);
        rep.bind(ep);

        while (running.load()) {
            zmq::message_t msg;
            auto result = rep.recv(msg, zmq::recv_flags::none);
            if (!result) continue;
            zmq::message_t reply(4);
            std::memcpy(reply.data(), "pong", 4);
            rep.send(reply, zmq::send_flags::none);
        }
    });

    // Give server time to bind
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ConnectionManager mgr;
    bool ok = mgr.probe(ep, 2000);
    REQUIRE(ok);

    running.store(false);
    server.join();
}
#endif // MICROBOTICA_HAS_ZMQ
