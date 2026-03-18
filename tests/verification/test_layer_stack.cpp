#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"
#include "test_config.h"

#ifdef MICROBOTICA_HAS_USD
#include "scene/scene_manager.h"
#include "core/result_frame.h"
#include "core/physics_config.h"
#include <pxr/usd/sdf/layer.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

// ── MBCA-VER-001 ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_001,
    "MBCA-VER-001",
    "MBCA-COMP-010",
    microbotica::core::BenchmarkType::LayerSeparation,
    "Base layer is never written by ResultsApplicator",
    __FILE__,
    "Base layer is never written by ResultsApplicator"
)

TEST_CASE("Base layer is never written by ResultsApplicator",
          "[verification][layer-separation]") {
#ifdef MICROBOTICA_HAS_USD
    microbotica::scene::SceneManager mgr;
    REQUIRE(mgr.loadScene(microbotica::test::fixturePath("test_scene.usda")));

    auto baseLayer = mgr.baseLayer();
    std::string baseContentBefore;
    baseLayer->ExportToString(&baseContentBefore);

    // Apply a result frame
    microbotica::core::ResultFrame frame;
    frame.simTime = 1.0;
    frame.positions["robot"] = {1.0, 2.0, 3.0};

    microbotica::core::PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";

    mgr.applyResultFrame(frame, config);

    std::string baseContentAfter;
    baseLayer->ExportToString(&baseContentAfter);

    REQUIRE(baseContentBefore == baseContentAfter);
#else
    WARN("USD not available — MBCA-VER-001 skipped");
#endif
}

// ── MBCA-VER-002 ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_002,
    "MBCA-VER-002",
    "MBCA-COMP-010",
    microbotica::core::BenchmarkType::LayerSeparation,
    "Results layer is not persisted to disk",
    __FILE__,
    "Results layer is not persisted to disk"
)

TEST_CASE("Results layer is not persisted to disk",
          "[verification][layer-separation]") {
#ifdef MICROBOTICA_HAS_USD
    microbotica::scene::SceneManager mgr;
    REQUIRE(mgr.loadScene(microbotica::test::fixturePath("test_scene.usda")));

    auto resultsLayer = mgr.resultsLayer();
    REQUIRE(resultsLayer);

    // Results layer should be anonymous (no file path)
    REQUIRE(resultsLayer->IsAnonymous());

    // Save override layer — results layer should not be affected
    mgr.saveOverrideLayer("/tmp/test_override.usd");

    // Results layer is still anonymous
    REQUIRE(resultsLayer->IsAnonymous());
#else
    WARN("USD not available — MBCA-VER-002 skipped");
#endif
}
