#include "scene/results_applicator.h"
#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/primSpec.h>
#endif

namespace microbotica::scene {

#ifdef MICROBOTICA_HAS_USD

ResultsApplicator::ResultsApplicator(SdfLayerRefPtr resultsLayer,
                                       UsdStageRefPtr stage)
    : resultsLayer_(std::move(resultsLayer))
    , stage_(std::move(stage))
{
}

void ResultsApplicator::apply(const core::ResultFrame& frame,
                               const core::PhysicsConfig& config)
{
    if (!resultsLayer_ || !stage_) return;

    // Use UsdEditContext to direct all writes to the results layer.
    // The results sublayer is inserted at index 0 (strongest opinion).
    UsdEditContext ctx(stage_, resultsLayer_);

    for (const auto& [actor, pos] : frame.positions) {
        auto it = config.actorToPrimPath.find(actor);
        if (it == config.actorToPrimPath.end()) {
            spdlog::warn("ResultsApplicator: No prim path mapping for actor '{}' — skipping", actor);
            continue;
        }

        const auto& primPath = it->second;
        auto prim = stage_->GetPrimAtPath(SdfPath(primPath));
        if (!prim.IsValid()) {
            spdlog::warn("ResultsApplicator: Unknown prim path '{}' for actor '{}' — data dropped",
                         primPath, actor);
            continue;
        }

        UsdGeomXformable xformable(prim);
        if (!xformable) {
            spdlog::warn("ResultsApplicator: Prim '{}' is not Xformable — cannot set translate",
                         primPath);
            continue;
        }

        // Get or create translate op on the results layer
        bool resetXformStack = false;
        auto ops = xformable.GetOrderedXformOps(&resetXformStack);

        UsdGeomXformOp translateOp;
        for (auto& op : ops) {
            if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
                translateOp = op;
                break;
            }
        }
        if (!translateOp) {
            translateOp = xformable.AddTranslateOp();
        }

        translateOp.Set(GfVec3d(pos.x, pos.y, pos.z));
    }

    // Write scalars as custom attributes on the default prim
    for (const auto& [name, value] : frame.scalars) {
        auto rootPrim = stage_->GetDefaultPrim();
        if (rootPrim.IsValid()) {
            auto attr = rootPrim.CreateAttribute(
                TfToken("microbotica:" + name),
                SdfValueTypeNames->Double,
                /* custom */ true);
            attr.Set(value);
        }
    }
}

#else

// No-USD stub
void ResultsApplicator::apply(const core::ResultFrame& /*frame*/,
                               const core::PhysicsConfig& /*config*/)
{
    spdlog::debug("ResultsApplicator: USD not available, apply is a no-op");
}

#endif

} // namespace microbotica::scene
