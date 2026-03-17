#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "stubs/stub_physics_process.h"

using namespace microbotica::core;
using namespace microbotica::stubs;

TEST_CASE("StubPhysicsProcess: meta is valid", "[unit][stub-physics]") {
    const auto& m = StubPhysicsProcess::meta();
    REQUIRE(m.component_id == "MBCA-IMPL-001");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

TEST_CASE("StubPhysicsProcess: starts idle", "[unit][stub-physics]") {
    StubPhysicsProcess proc;
    REQUIRE(proc.status() == ProcessStatus::Idle);
}

TEST_CASE("StubPhysicsProcess: launch and receive 10 frames", "[unit][stub-physics]") {
    StubPhysicsProcess proc;
    PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";

    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    double lastTime = -1.0;
    for (int i = 0; i < 10; ++i) {
        auto frame = proc.receiveResult();
        REQUIRE(frame.has_value());
        REQUIRE(frame->simTime > lastTime);
        lastTime = frame->simTime;
        REQUIRE(frame->positions.count("robot") == 1);
        REQUIRE(frame->scalars.count("step_out_frequency") == 1);

        // Verify no NaN or Inf
        const auto& pos = frame->positions.at("robot");
        REQUIRE_FALSE(std::isnan(pos.x));
        REQUIRE_FALSE(std::isnan(pos.y));
        REQUIRE_FALSE(std::isnan(pos.z));
        REQUIRE_FALSE(std::isinf(pos.x));
        REQUIRE_FALSE(std::isinf(pos.y));
        REQUIRE_FALSE(std::isinf(pos.z));
    }

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);
}

TEST_CASE("StubPhysicsProcess: status transitions", "[unit][stub-physics]") {
    StubPhysicsProcess proc;
    REQUIRE(proc.status() == ProcessStatus::Idle);

    PhysicsConfig config;
    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);
}
