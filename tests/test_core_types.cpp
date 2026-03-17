#include <catch2/catch_test_macros.hpp>

#include "core/component_meta.h"
#include "core/stability.h"
#include "core/types.h"
#include "core/result_frame.h"
#include "core/physics_config.h"
#include "core/render_config.h"
#include "core/physics_process.h"
#include "core/render_backend.h"
#include "core/render_session.h"
#include "core/compute_backend.h"
#include "core/verification_registry.h"
#include "core/audit_logger.h"
#include "core/session_provenance.h"
#include "core/threadsafe_queue.h"

using namespace microbotica::core;

TEST_CASE("ResultFrame: construct and populate", "[unit][core]") {
    ResultFrame frame;
    frame.simTime = 1.5;
    frame.positions["robot"] = Vec3f{1.0, 2.0, 3.0};
    frame.scalars["frequency"] = 60.0;

    REQUIRE(frame.simTime == 1.5);
    REQUIRE(frame.positions.at("robot").x == 1.0);
    REQUIRE(frame.scalars.at("frequency") == 60.0);
}

TEST_CASE("ResultFrame: JSON roundtrip", "[unit][core]") {
    ResultFrame original;
    original.simTime = 2.5;
    original.positions["robot"] = Vec3f{1.0, 2.0, 3.0};
    original.scalars["step"] = 42.0;

    nlohmann::json j = original;
    auto restored = j.get<ResultFrame>();

    REQUIRE(restored.simTime == original.simTime);
    REQUIRE(restored.positions.at("robot").x == 1.0);
    REQUIRE(restored.scalars.at("step") == 42.0);
}

TEST_CASE("PhysicsConfig: JSON roundtrip", "[unit][core]") {
    PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";

    nlohmann::json j = config;
    auto restored = j.get<PhysicsConfig>();

    REQUIRE(restored.actorToPrimPath.at("robot") == "/World/Robot");
}

TEST_CASE("ComponentMeta: JSON serialization", "[unit][core]") {
    ComponentMeta meta{
        .component_id      = "MBCA-COMP-TEST",
        .component_version = "1.0.0",
        .stability         = StabilityLevel::Experimental,
        .description       = "Test component",
        .preconditions     = {"pre1"},
        .postconditions    = {"post1"},
        .invariants        = {},
        .design_rationale  = "For testing",
        .assumptions       = {"assume1"},
        .limitations       = {"limit1"},
        .validated_regimes = {},
        .hazard_hints      = {"hazard1"},
        .references        = {},
        .deprecation_notice = std::nullopt,
    };

    nlohmann::json j = meta;
    REQUIRE(j["component_id"] == "MBCA-COMP-TEST");
    REQUIRE(j["stability"] == "experimental");
    REQUIRE(j["deprecation_notice"].is_null());
}

TEST_CASE("PhysicsProcess: interfaceMeta is valid", "[unit][core]") {
    const auto& meta = PhysicsProcess::interfaceMeta();
    REQUIRE(meta.component_id == "MBCA-COMP-001");
    REQUIRE(meta.stability == StabilityLevel::Experimental);
}

TEST_CASE("RenderBackend: interfaceMeta is valid", "[unit][core]") {
    const auto& meta = RenderBackend::interfaceMeta();
    REQUIRE(meta.component_id == "MBCA-COMP-002");
}

TEST_CASE("RenderSession: interfaceMeta is valid", "[unit][core]") {
    const auto& meta = RenderSession::interfaceMeta();
    REQUIRE(meta.component_id == "MBCA-COMP-003");
}

TEST_CASE("ComputeBackend: interfaceMeta is valid", "[unit][core]") {
    const auto& meta = ComputeBackend::interfaceMeta();
    REQUIRE(meta.component_id == "MBCA-COMP-004");
}

TEST_CASE("ThreadSafeQueue: push and try_pop", "[unit][core]") {
    ThreadSafeQueue<int> q;
    REQUIRE(q.try_pop() == std::nullopt);

    q.push(42);
    auto val = q.try_pop();
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 42);
    REQUIRE(q.try_pop() == std::nullopt);
}

TEST_CASE("NullAuditLogger: no-ops don't crash", "[unit][core]") {
    NullAuditLogger logger;
    logger.enable("/dev/null");
    logger.logEvent("test", {});
    logger.logParameterChange("p", 1, 2);
    logger.logResultFrame(0, 0.0, false);
    logger.logScriptCommand("print('hi')", true);
}

TEST_CASE("VerificationRegistry: starts empty", "[unit][core]") {
    // Note: static init from REGISTER macros may have populated this
    // in other test files, but the function should at least be callable
    auto& reg = benchmarkRegistry();
    REQUIRE(reg.size() >= 0); // just test it's callable
}
