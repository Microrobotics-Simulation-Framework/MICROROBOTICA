// A live session's frames on a MIME 0.3-shaped stage: the binding's poses written in the stage's units, a field
// vector's arrow scaled, turned and shown, the results layer the only one written; MIME's visuals overlay placed over
// the stage and under the user's overrides.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/verification_registry.h"

#ifdef MICROBOTICA_HAS_USD
#include "live/binding_frames.h"
#include "scene/scene_manager.h"

#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
PXR_NAMESPACE_USING_DIRECTIVE

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// A stage written as MIME 0.3 writes one, in millimetres: a body placed in the world, a chain frame resetting its
// transform stack, and a field vector's prim (hidden, an arrow of one metre: 1000 mm) with its metres per unit.
const char* kStage = R"(#usda 1.0
(
    defaultPrim = "World"
    metersPerUnit = 0.001
    upAxis = "Z"
)
def Xform "World"
{
    def Xform "umr"
    {
        double3 xformOp:translate = (0, 0, -1)
        quatd xformOp:orient = (1, 0, 0, 0)
        uniform token[] xformOpOrder = ["xformOp:translate", "xformOp:orient"]
        def Sphere "shell"
        {
            double radius = 0.5
        }
    }
    def Scope "rpm"
    {
        def Xform "arm"
        {
            def Xform "link_1"
            {
                double3 xformOp:translate = (0, 0, 100)
                quatd xformOp:orient = (1, 0, 0, 0)
                uniform token[] xformOpOrder = ["!resetXformStack!", "xformOp:translate", "xformOp:orient"]
            }
        }
        def Scope "magnetic"
        {
            def Xform "magnetic_torque_umr"
            {
                double3 xformOp:translate = (0, 0, -1)
                quatd xformOp:orient = (1, 0, 0, 0)
                double3 xformOp:scale = (1, 1, 1)
                uniform token[] xformOpOrder = ["!resetXformStack!", "xformOp:translate", "xformOp:orient", "xformOp:scale"]
                token visibility = "invisible"
                double mime:vector:metres_per_unit = 2.5
                def Cylinder "arrow"
                {
                    double height = 1000
                }
            }
        }
    }
}
)";

std::string writeTemp(const std::string& name, const std::string& text) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path) << text;
    return path.string();
}

microbotica::live::FrameSchema schema() {
    using microbotica::live::FieldSpec;
    microbotica::live::FrameSchema s;
    s.fields.push_back(FieldSpec{"umr.pose", "umr", "pose", {7}, "<f4", "m; 1", 0});
    s.fields.push_back(FieldSpec{"rpm.arm.frame_poses", "rpm.arm", "frame_poses", {2, 7}, "<f4", "m; 1", 28});
    s.fields.push_back(FieldSpec{"rpm.magnetic.b", "rpm.magnetic", "b", {3}, "<f4", "T", 84});
    return s;
}

std::vector<microbotica::live::BindingEntry> binding() {
    using microbotica::live::BindingEntry;
    std::vector<BindingEntry> b(3);
    b[0].prim = "/World/umr"; b[0].node = "umr"; b[0].field = "pose"; b[0].kind = "pose";
    b[1].prim = "/World/rpm/arm/link_1"; b[1].node = "rpm.arm"; b[1].field = "frame_poses"; b[1].index = 1;
    b[1].kind = "pose";
    b[2].prim = "/World/rpm/magnetic/magnetic_torque_umr"; b[2].node = "rpm.magnetic"; b[2].field = "b";
    b[2].kind = "vector"; b[2].atNode = "umr"; b[2].atField = "pose";
    return b;
}

GfVec3d translateOf(const UsdPrim& prim) {
    for (const auto& op : UsdGeomXformable(prim).GetOrderedXformOps(nullptr)) {
        if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
            GfVec3d v;
            op.Get(&v);
            return v;
        }
    }
    return GfVec3d(NAN);
}

} // namespace
#endif

REGISTER_VERIFICATION_BENCHMARK(
    MBCA_VER_016,
    "MBCA-VER-016",
    "MBCA-COMP-011",
    microbotica::core::BenchmarkType::DataIntegrity,
    "A live frame's poses land in the stage's units, its field vector's arrow scaled, turned and shown, on the "
    "results layer only",
    __FILE__,
    "Live frames on a millimetre stage: poses in its units, the arrow scaled and shown, the results layer only"
)

TEST_CASE("Live frames on a millimetre stage: poses in its units, the arrow scaled and shown, the results layer only",
          "[verification][data-integrity][live]") {
#ifdef MICROBOTICA_HAS_USD
    microbotica::scene::SceneManager mgr;
    REQUIRE(mgr.loadScene(writeTemp("microrobotica_live_stage.usda", kStage)));
    CHECK(mgr.metersPerUnit() == Catch::Approx(0.001));
    const auto perUnit = mgr.vectorMetresPerUnit();
    REQUIRE(perUnit.count("/World/rpm/magnetic/magnetic_torque_umr") == 1);
    CHECK(perUnit.at("/World/rpm/magnetic/magnetic_torque_umr") == 2.5);

    microbotica::live::StageUnits units{mgr.metersPerUnit(), perUnit};
    const microbotica::live::BindingFrames frames(binding(), schema(), units);
    REQUIRE(frames.unbound().empty());

    microbotica::live::LiveFrame f;
    f.time = 0.5;
    f.values["umr.pose"] = {0.002, -0.001, -0.0015, 1, 0, 0, 0};  // metres
    f.values["rpm.arm.frame_poses"] = {0, 0, 0, 1, 0, 0, 0, 0.1, 0.2, 0.3, 1, 0, 0, 0};
    f.values["rpm.magnetic.b"] = {0.0, 0.0006, 0.0008};  // 1 mT along (0, 0.6, 0.8)
    mgr.applyResultFrame(frames.toResultFrame(f), frames.physicsConfig());

    const auto stage = mgr.stage();
    const GfVec3d body = translateOf(stage->GetPrimAtPath(SdfPath("/World/umr")));
    CHECK(body[0] == Catch::Approx(2.0));    // mm
    CHECK(body[2] == Catch::Approx(-1.5));
    const GfVec3d link = translateOf(stage->GetPrimAtPath(SdfPath("/World/rpm/arm/link_1")));
    CHECK(link[1] == Catch::Approx(200.0));  // row 1 of the frame poses, in mm

    const UsdPrim arrow = stage->GetPrimAtPath(SdfPath("/World/rpm/magnetic/magnetic_torque_umr"));
    CHECK(translateOf(arrow)[0] == Catch::Approx(2.0));  // at the body
    GfVec3d scale(0.0);
    GfQuatd orient;
    for (const auto& op : UsdGeomXformable(arrow).GetOrderedXformOps(nullptr)) {
        if (op.GetOpType() == UsdGeomXformOp::TypeScale) op.Get(&scale);
        if (op.GetOpType() == UsdGeomXformOp::TypeOrient) op.Get(&orient);
    }
    CHECK(scale[0] == Catch::Approx(0.001 * 2.5));  // |B| x metres per tesla: the arrow's length in metres
    const GfVec3d along = orient.Transform(GfVec3d(0, 0, 1));
    CHECK(along[1] == Catch::Approx(0.6));
    CHECK(along[2] == Catch::Approx(0.8));
    TfToken visibility;
    UsdGeomImageable(arrow).GetVisibilityAttr().Get(&visibility);
    CHECK(visibility == UsdGeomTokens->inherited);

    // Written to the results layer only: the base layer still holds the stage as it was.
    const auto baseSpec = mgr.baseLayer()->GetAttributeAtPath(
        SdfPath("/World/rpm/magnetic/magnetic_torque_umr.visibility"));
    REQUIRE(baseSpec);
    CHECK(baseSpec->GetDefaultValue().Get<TfToken>() == UsdGeomTokens->invisible);
    CHECK(mgr.resultsLayer()->GetAttributeAtPath(
        SdfPath("/World/rpm/magnetic/magnetic_torque_umr.xformOp:scale")));
#else
    SKIP("OpenUSD not available");
#endif
}

TEST_CASE("MIME's visuals overlay goes over the stage and under the user's overrides", "[unit][live]") {
#ifdef MICROBOTICA_HAS_USD
    microbotica::scene::SceneManager mgr;
    REQUIRE(mgr.loadScene(writeTemp("microrobotica_overlay_stage.usda", kStage)));
    const std::string overlay = writeTemp("microrobotica_overlay.usda", R"(#usda 1.0
over "World"
{
    over "umr"
    {
        over "shell"
        {
            double radius = 2.0
        }
    }
}
)");
    REQUIRE(mgr.setOverlayLayer(overlay));
    const std::vector<std::string> stack = mgr.stage()->GetRootLayer()->GetSubLayerPaths();
    REQUIRE(stack.size() == 4);
    CHECK(stack[0] == mgr.resultsLayer()->GetIdentifier());
    CHECK(stack[1] == mgr.overrideLayer()->GetIdentifier());
    CHECK(stack[2] == mgr.overlayLayerPath());
    CHECK(stack[3] == mgr.baseLayer()->GetIdentifier());
    double radius = 0.0;
    mgr.stage()->GetPrimAtPath(SdfPath("/World/umr/shell")).GetAttribute(TfToken("radius")).Get(&radius);
    CHECK(radius == 2.0);  // the overlay is stronger than the stage
    // Setting it again replaces it rather than stacking a second.
    REQUIRE(mgr.setOverlayLayer(overlay));
    CHECK(mgr.stage()->GetRootLayer()->GetSubLayerPaths().size() == 4);
#else
    SKIP("OpenUSD not available");
#endif
}
