#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>

#include "connection/connection_config.h"
#include "connection/connection_manager.h"
#include "mime_adapter/mime_physics_process.h"
#include "mime_adapter/mime_compute_backend.h"

using namespace microbotica::connection;
using namespace microbotica::mime;
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

// ── MimePhysicsProcess tests ─────────────────────────────────────────────────

TEST_CASE("MimePhysicsProcess: meta is valid", "[unit][mime]") {
    const auto& m = MimePhysicsProcess::meta();
    REQUIRE(m.component_id == "MBCA-IMPL-010");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

TEST_CASE("MimePhysicsProcess: starts idle", "[unit][mime]") {
    MimePhysicsProcess proc("tcp://localhost:5555", "tcp://localhost:5556");
    REQUIRE(proc.status() == ProcessStatus::Idle);
}

// ── MimeComputeBackend tests ─────────────────────────────────────────────────

TEST_CASE("MimeComputeBackend: meta is valid", "[unit][mime]") {
    const auto& m = MimeComputeBackend::meta();
    REQUIRE(m.component_id == "MBCA-IMPL-011");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

TEST_CASE("MimeComputeBackend: backendName", "[unit][mime]") {
    ConnectionConfig config;
    MimeComputeBackend backend(config);
    REQUIRE(backend.backendName() == "mime");
}

TEST_CASE("MimeComputeBackend: createPhysicsProcess returns MimePhysicsProcess", "[unit][mime]") {
    ConnectionConfig config;
    config.mode = ConnectionMode::Manual;
    config.endpoint = "tcp://localhost:5556";
    config.zmq_req_port = 5555;

    MimeComputeBackend backend(config);
    auto proc = backend.createPhysicsProcess();
    REQUIRE(proc != nullptr);
    REQUIRE(proc->status() == ProcessStatus::Idle);
}

// ── ZMQ mock integration test ────────────────────────────────────────────────

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>

TEST_CASE("MimePhysicsProcess: receives frames from mock ZMQ publisher", "[integration][mime]") {
    // Use high ports to avoid conflicts
    const std::string req_ep = "tcp://127.0.0.1:15555";
    const std::string pub_ep = "tcp://127.0.0.1:15556";

    // Start mock REP server (responds to "launch" with "ok")
    std::atomic<bool> rep_running{true};
    std::thread rep_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 1000);
        rep.bind(req_ep);

        while (rep_running.load()) {
            zmq::message_t msg;
            auto result = rep.recv(msg, zmq::recv_flags::none);
            if (!result) continue;
            std::string cmd(static_cast<char*>(msg.data()), msg.size());
            zmq::message_t reply(2);
            std::memcpy(reply.data(), "ok", 2);
            rep.send(reply, zmq::send_flags::none);
        }
    });

    // Start mock PUB server (publishes 5 ResultFrame JSON messages)
    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(pub_ep);

        // Give subscribers time to connect
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        for (int i = 0; i < 5; ++i) {
            ResultFrame frame;
            frame.simTime = static_cast<double>(i) * 0.1;
            frame.positions["robot"] = Vec3f{1.0 * i, 2.0, 3.0};
            frame.orientations["robot"] = Quatf{1.0, 0.0, 0.0, 0.0};

            nlohmann::json j = frame;
            std::string data = j.dump();
            zmq::message_t msg(data.size());
            std::memcpy(msg.data(), data.data(), data.size());
            pub.send(msg, zmq::send_flags::none);

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Create and launch MimePhysicsProcess
    MimePhysicsProcess proc(req_ep, pub_ep);
    PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";
    proc.launch(config);

    REQUIRE(proc.status() == ProcessStatus::Running);

    // Receive at least 3 frames (some may be lost during SUB connection)
    int received = 0;
    double lastTime = -1.0;
    for (int attempt = 0; attempt < 10 && received < 3; ++attempt) {
        auto frame = proc.receiveResult();
        if (frame && frame->simTime >= 0.0) {
            REQUIRE(frame->simTime >= lastTime);
            lastTime = frame->simTime;
            REQUIRE(frame->positions.count("robot") == 1);
            REQUIRE(frame->orientations.count("robot") == 1);
            received++;
        }
    }

    REQUIRE(received >= 3);

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);

    rep_running.store(false);
    rep_thread.join();
    pub_thread.join();
}

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
