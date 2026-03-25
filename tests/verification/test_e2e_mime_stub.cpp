#include <catch2/catch_test_macros.hpp>
#include "core/verification_registry.h"
#include "mime_adapter/mime_stub_physics_process.h"
#include "test_config.h"

#ifdef MICROBOTICA_HAS_USD
#include "scene/scene_manager.h"
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

// ── MBCA-VER-010 ──────────────────────────────────────────────────────────────

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_010,
    "MBCA-VER-010",
    "MBCA-IMPL-012",
    microbotica::core::BenchmarkType::DataIntegrity,
    "MimeStubPhysicsProcess data arrives at USD prims via demo scene pipeline",
    __FILE__,
    "MimeStubPhysicsProcess data arrives at USD prims via demo scene pipeline"
)

TEST_CASE("MimeStubPhysicsProcess data arrives at USD prims via demo scene pipeline",
          "[verification][data-integrity]") {
#ifdef MICROBOTICA_HAS_USD
    // Load the UMR confinement demo scene
    microbotica::scene::SceneManager mgr;
    REQUIRE(mgr.loadScene(microbotica::test::fixturePath("umr_confinement.usda")));

    // Launch MimeStubPhysicsProcess
    microbotica::mime::MimeStubPhysicsProcess proc;
    microbotica::core::PhysicsConfig config;
    config.actorToPrimPath["rigid_body"] = "/World/Actors/UMR";
    config.actorToPrimPath["ext_field"] = "/World/Actors/FieldArrow";
    config.actorToPrimPath["flow_field"] = "/World/Analysis/FlowField";

    proc.launch(config);
    REQUIRE(proc.status() == microbotica::core::ProcessStatus::Running);

    // Receive one frame and apply it
    auto frame = proc.receiveResult();
    REQUIRE(frame.has_value());
    REQUIRE(frame->simTime >= 0.0);

    mgr.applyResultFrame(*frame, config);

    // Verify UMR position was written
    auto umr_prim = mgr.stage()->GetPrimAtPath(SdfPath("/World/Actors/UMR"));
    REQUIRE(umr_prim.IsValid());
    UsdGeomXformable umr_xform(umr_prim);
    REQUIRE(umr_xform);

    bool resetXformStack = false;
    auto umr_ops = umr_xform.GetOrderedXformOps(&resetXformStack);

    // Find translate op and verify non-zero position (circular orbit)
    UsdGeomXformOp translateOp;
    for (auto& op : umr_ops) {
        if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
            translateOp = op;
            break;
        }
    }
    REQUIRE(translateOp);
    GfVec3d translate;
    translateOp.Get(&translate);
    // At t=0, position is (0.003, 0, 0) — X should be ~0.003
    REQUIRE(translate[0] != 0.0); // not the initial (0,0,0)

    // Find orient op and verify
    UsdGeomXformOp orientOp;
    for (auto& op : umr_ops) {
        if (op.GetOpType() == UsdGeomXformOp::TypeOrient) {
            orientOp = op;
            break;
        }
    }
    REQUIRE(orientOp);
    GfQuatd orient;
    orientOp.Get(&orient);
    // Quaternion should be valid (non-zero)
    REQUIRE((orient.GetReal() != 0.0 || orient.GetImaginary().GetLength() != 0.0));

    // Verify FieldArrow orient was written
    auto arrow_prim = mgr.stage()->GetPrimAtPath(SdfPath("/World/Actors/FieldArrow"));
    REQUIRE(arrow_prim.IsValid());
    UsdGeomXformable arrow_xform(arrow_prim);
    bool reset2 = false;
    auto arrow_ops = arrow_xform.GetOrderedXformOps(&reset2);
    UsdGeomXformOp arrow_orient;
    for (auto& op : arrow_ops) {
        if (op.GetOpType() == UsdGeomXformOp::TypeOrient) {
            arrow_orient = op;
            break;
        }
    }
    REQUIRE(arrow_orient);

    // Verify FlowField vertex colors were written
    auto flow_prim = mgr.stage()->GetPrimAtPath(SdfPath("/World/Analysis/FlowField"));
    REQUIRE(flow_prim.IsValid());
    auto primvarsAPI = UsdGeomPrimvarsAPI(flow_prim);
    auto displayColorPv = primvarsAPI.GetPrimvar(TfToken("displayColor"));
    REQUIRE(displayColorPv);

    VtVec3fArray colors;
    displayColorPv.Get(&colors);
    REQUIRE(colors.size() == 4);

    // Colors should be in [0,1] range (velocity-coded blue↔red)
    for (size_t i = 0; i < colors.size(); ++i) {
        REQUIRE(colors[i][0] >= 0.0f);
        REQUIRE(colors[i][0] <= 1.0f);
        REQUIRE(colors[i][2] >= 0.0f);
        REQUIRE(colors[i][2] <= 1.0f);
    }

    proc.stop();
#else
    WARN("USD not available — MBCA-VER-010 skipped");
#endif
}
