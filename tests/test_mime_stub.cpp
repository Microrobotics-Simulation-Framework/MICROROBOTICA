#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "mime_adapter/mime_stub_physics_process.h"

using namespace microbotica::core;
using namespace microbotica::mime;

TEST_CASE("MimeStubPhysicsProcess: meta is valid", "[unit][mime-stub]") {
    const auto& m = MimeStubPhysicsProcess::meta();
    REQUIRE(m.component_id == "MBCA-IMPL-012");
    REQUIRE(m.stability == StabilityLevel::Experimental);
}

TEST_CASE("MimeStubPhysicsProcess: starts idle", "[unit][mime-stub]") {
    MimeStubPhysicsProcess proc;
    REQUIRE(proc.status() == ProcessStatus::Idle);
}

TEST_CASE("MimeStubPhysicsProcess: launch and receive 5 frames", "[unit][mime-stub]") {
    MimeStubPhysicsProcess proc;
    PhysicsConfig config;
    config.actorToPrimPath["rigid_body"] = "/World/Actors/UMR";
    config.actorToPrimPath["ext_field"] = "/World/Actors/FieldArrow";
    config.actorToPrimPath["flow_field"] = "/World/Analysis/FlowField";

    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    double lastTime = -1.0;
    for (int i = 0; i < 5; ++i) {
        auto frame = proc.receiveResult();
        REQUIRE(frame.has_value());
        REQUIRE(frame->simTime > lastTime);
        lastTime = frame->simTime;

        // Positions
        REQUIRE(frame->positions.count("rigid_body") == 1);
        REQUIRE(frame->positions.count("ext_field") == 1);

        // Orientations
        REQUIRE(frame->orientations.count("rigid_body") == 1);
        REQUIRE(frame->orientations.count("ext_field") == 1);

        // Mesh data
        REQUIRE(frame->meshes.count("flow_field") == 1);
        REQUIRE(frame->meshes.at("flow_field").vertexColors.size() == 4);

        // No NaN in positions
        const auto& pos = frame->positions.at("rigid_body");
        REQUIRE_FALSE(std::isnan(pos.x));
        REQUIRE_FALSE(std::isnan(pos.y));
        REQUIRE_FALSE(std::isnan(pos.z));

        // No NaN in orientations
        const auto& q = frame->orientations.at("rigid_body");
        REQUIRE_FALSE(std::isnan(q.w));
        REQUIRE_FALSE(std::isnan(q.x));
        REQUIRE_FALSE(std::isnan(q.y));
        REQUIRE_FALSE(std::isnan(q.z));

        // Vertex colors in [0,1] range
        for (const auto& c : frame->meshes.at("flow_field").vertexColors) {
            REQUIRE(c.x >= 0.0);
            REQUIRE(c.x <= 1.0);
            REQUIRE(c.z >= 0.0);
            REQUIRE(c.z <= 1.0);
        }

        // Scalars
        REQUIRE(frame->scalars.count("step_out_frequency") == 1);
    }

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);
}

TEST_CASE("MimeStubPhysicsProcess: status transitions", "[unit][mime-stub]") {
    MimeStubPhysicsProcess proc;
    REQUIRE(proc.status() == ProcessStatus::Idle);

    PhysicsConfig config;
    proc.launch(config);
    REQUIRE(proc.status() == ProcessStatus::Running);

    proc.stop();
    REQUIRE(proc.status() == ProcessStatus::Stopped);
}
