#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "core/verification_registry.h"
#include "stubs/stub_physics_process.h"

// ── MBCA-VER-006 ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_006,
    "MBCA-VER-006",
    "MBCA-COMP-001",
    microbotica::core::BenchmarkType::Regression,
    "100 frames from StubPhysicsProcess: simTime monotonic, no NaN/Inf",
    __FILE__,
    "StubPhysicsProcess: 100 frames monotonic, no NaN/Inf"
)

TEST_CASE("StubPhysicsProcess: 100 frames monotonic, no NaN/Inf",
          "[verification][regression]") {
    using namespace microbotica::core;
    using namespace microbotica::stubs;

    StubPhysicsProcess proc;
    PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";

    proc.launch(config);

    double lastTime = -1.0;
    for (int i = 0; i < 100; ++i) {
        auto frame = proc.receiveResult();
        REQUIRE(frame.has_value());

        // simTime must be monotonically increasing
        REQUIRE(frame->simTime > lastTime);
        lastTime = frame->simTime;

        // No NaN or Inf in any position
        for (const auto& [actor, pos] : frame->positions) {
            REQUIRE_FALSE(std::isnan(pos.x));
            REQUIRE_FALSE(std::isnan(pos.y));
            REQUIRE_FALSE(std::isnan(pos.z));
            REQUIRE_FALSE(std::isinf(pos.x));
            REQUIRE_FALSE(std::isinf(pos.y));
            REQUIRE_FALSE(std::isinf(pos.z));
        }

        // No NaN or Inf in scalars
        for (const auto& [name, val] : frame->scalars) {
            REQUIRE_FALSE(std::isnan(val));
            REQUIRE_FALSE(std::isinf(val));
        }
    }

    proc.stop();
}
