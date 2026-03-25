#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"
#include "core/result_frame.h"

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#include <thread>
#include <chrono>
#endif

// ── MBCA-VER-009 ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_009,
    "MBCA-VER-009",
    "MBCA-IMPL-010",
    microbotica::core::BenchmarkType::ProtocolFidelity,
    "ResultFrame JSON survives ZMQ PUB/SUB transport without corruption",
    __FILE__,
    "ResultFrame JSON survives ZMQ PUB/SUB transport without corruption"
)

TEST_CASE("ResultFrame JSON survives ZMQ PUB/SUB transport without corruption",
          "[verification][protocol-fidelity]") {
#ifdef MICROBOTICA_HAS_ZMQ
    using namespace microbotica::core;

    const std::string endpoint = "tcp://127.0.0.1:15558";

    // Build a ResultFrame with all field types populated
    ResultFrame original;
    original.simTime = 3.14159265358979;
    original.positions["robot"] = Vec3f{1.23456789, -2.34567890, 3.45678901};
    original.orientations["robot"] = Quatf{0.5, 0.5, 0.5, 0.5};
    original.scalars["frequency"] = 42.195;
    original.meshes["flow"] = MeshData{
        {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}
    };

    nlohmann::json sent_json = original;
    std::string sent_data = sent_json.dump();

    // PUB thread: serialize and send
    std::thread pub_thread([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t pub(ctx, zmq::socket_type::pub);
        pub.set(zmq::sockopt::linger, 0);
        pub.bind(endpoint);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        zmq::message_t msg(sent_data.size());
        std::memcpy(msg.data(), sent_data.data(), sent_data.size());
        pub.send(msg, zmq::send_flags::none);

        // Send a few more times for reliability (SUB may miss the first)
        for (int i = 0; i < 3; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            zmq::message_t msg2(sent_data.size());
            std::memcpy(msg2.data(), sent_data.data(), sent_data.size());
            pub.send(msg2, zmq::send_flags::none);
        }
    });

    // SUB: receive and deserialize
    zmq::context_t ctx(1);
    zmq::socket_t sub(ctx, zmq::socket_type::sub);
    sub.set(zmq::sockopt::linger, 0);
    sub.set(zmq::sockopt::rcvtimeo, 5000);
    sub.set(zmq::sockopt::subscribe, "");
    sub.connect(endpoint);

    zmq::message_t received_msg;
    auto result = sub.recv(received_msg, zmq::recv_flags::none);
    REQUIRE(result.has_value());

    std::string received_data(static_cast<char*>(received_msg.data()), received_msg.size());

    // Parse received data
    auto received_json = nlohmann::json::parse(received_data);
    auto received = received_json.get<ResultFrame>();

    // Verify exact fidelity — no corruption through the transport
    REQUIRE(received.simTime == original.simTime);

    REQUIRE(received.positions.at("robot").x == original.positions.at("robot").x);
    REQUIRE(received.positions.at("robot").y == original.positions.at("robot").y);
    REQUIRE(received.positions.at("robot").z == original.positions.at("robot").z);

    REQUIRE(received.orientations.at("robot").w == original.orientations.at("robot").w);
    REQUIRE(received.orientations.at("robot").x == original.orientations.at("robot").x);
    REQUIRE(received.orientations.at("robot").y == original.orientations.at("robot").y);
    REQUIRE(received.orientations.at("robot").z == original.orientations.at("robot").z);

    REQUIRE(received.scalars.at("frequency") == original.scalars.at("frequency"));

    REQUIRE(received.meshes.at("flow").vertexColors.size() == 3);
    REQUIRE(received.meshes.at("flow").vertexColors[0].x == 1.0);
    REQUIRE(received.meshes.at("flow").vertexColors[1].y == 1.0);
    REQUIRE(received.meshes.at("flow").vertexColors[2].z == 1.0);

    pub_thread.join();
#else
    WARN("ZMQ not available — MBCA-VER-009 skipped");
#endif
}
