#include "scene/results_applicator.h"
#include <spdlog/spdlog.h>
#include "core/profiler.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/vec3f.h>
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
    MBCA_PROFILE_SCOPE("apply_frame");

    if (!resultsLayer_ || !stage_) return;

    // Use UsdEditContext to direct all writes to the results layer.
    UsdEditContext ctx(stage_, resultsLayer_);

    // Batch all attribute writes into a single USD change notification.
    // Without this, each Set() triggers recomposition separately — expensive.
    SdfChangeBlock changeBlock;

    // Write positions and orientations using cached xform op handles.
    // getCachedOps() populates the cache on first call and invalidates
    // when scene generation changes.
    for (const auto& [actor, pos] : frame.positions) {
        auto it = config.actorToPrimPath.find(actor);
        if (it == config.actorToPrimPath.end()) {
            spdlog::warn("ResultsApplicator: No prim path mapping for actor '{}' — skipping", actor);
            continue;
        }
        auto& cached = getCachedOps(it->second, cacheGeneration_);
        if (cached.valid && cached.translateOp) {
            cached.translateOp.Set(GfVec3d(pos.x, pos.y, pos.z));
        }
    }

    for (const auto& [actor, quat] : frame.orientations) {
        auto it = config.actorToPrimPath.find(actor);
        if (it == config.actorToPrimPath.end()) continue;
        auto& cached = getCachedOps(it->second, cacheGeneration_);
        if (cached.valid && cached.orientOp) {
            // Match the attribute's precision — MIME recordings use GfQuatf,
            // hand-authored .usda scenes may use GfQuatd.
            auto typeName = cached.orientOp.GetTypeName();
            if (typeName == SdfValueTypeNames->Quatf) {
                cached.orientOp.Set(GfQuatf(
                    static_cast<float>(quat.w),
                    GfVec3f(static_cast<float>(quat.x),
                            static_cast<float>(quat.y),
                            static_cast<float>(quat.z))));
            } else {
                cached.orientOp.Set(GfQuatd(quat.w, GfVec3d(quat.x, quat.y, quat.z)));
            }
        }
    }

    // Write mesh vertex colors as primvars:displayColor
    for (const auto& [actor, meshData] : frame.meshes) {
        auto it = config.actorToPrimPath.find(actor);
        if (it == config.actorToPrimPath.end()) {
            spdlog::warn("ResultsApplicator: No prim path mapping for actor '{}' — skipping mesh", actor);
            continue;
        }

        const auto& primPath = it->second;
        auto prim = stage_->GetPrimAtPath(SdfPath(primPath));
        if (!prim.IsValid()) {
            spdlog::warn("ResultsApplicator: Unknown prim path '{}' for actor '{}' — mesh data dropped",
                         primPath, actor);
            continue;
        }

        UsdGeomMesh mesh(prim);
        if (!mesh) {
            spdlog::warn("ResultsApplicator: Prim '{}' is not a Mesh — cannot set vertex colors",
                         primPath);
            continue;
        }

        // Validate vertex count matches mesh topology
        VtIntArray faceVertexIndices;
        mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
        if (!faceVertexIndices.empty()) {
            // Find max vertex index to determine expected vertex count
            int maxIdx = 0;
            for (const auto& idx : faceVertexIndices) {
                if (idx > maxIdx) maxIdx = idx;
            }
            size_t expectedVertexCount = static_cast<size_t>(maxIdx + 1);
            if (meshData.vertexColors.size() != expectedVertexCount) {
                spdlog::warn("ResultsApplicator: Vertex color count ({}) does not match "
                             "mesh topology ({} vertices) for prim '{}' — skipping",
                             meshData.vertexColors.size(), expectedVertexCount, primPath);
                continue;
            }
        }

        // Build Color3fArray from vertexColors
        VtVec3fArray colors(meshData.vertexColors.size());
        for (size_t i = 0; i < meshData.vertexColors.size(); ++i) {
            const auto& c = meshData.vertexColors[i];
            colors[i] = GfVec3f(static_cast<float>(c.x),
                                static_cast<float>(c.y),
                                static_cast<float>(c.z));
        }

        auto primvarsAPI = UsdGeomPrimvarsAPI(prim);
        auto displayColorPv = primvarsAPI.CreatePrimvar(
            TfToken("displayColor"),
            SdfValueTypeNames->Color3fArray,
            UsdGeomTokens->vertex);
        displayColorPv.Set(colors);
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

ResultsApplicator::CachedOps& ResultsApplicator::getCachedOps(
    const std::string& primPath, uint64_t currentGen)
{
    // Invalidate entire cache if scene generation changed
    if (cacheGeneration_ != currentGen) {
        opsCache_.clear();
        cacheGeneration_ = currentGen;
    }

    auto it = opsCache_.find(primPath);
    if (it != opsCache_.end() && it->second.valid) {
        // Safety net: check handles are still defined (survives hot-swap)
        if (it->second.translateOp && !it->second.translateOp.IsDefined()) {
            it->second.valid = false;
        }
        if (it->second.valid) return it->second;
    }

    // Populate cache for this prim
    CachedOps ops;
    auto prim = stage_->GetPrimAtPath(SdfPath(primPath));
    if (prim.IsValid()) {
        UsdGeomXformable xformable(prim);
        if (xformable) {
            bool resetXformStack = false;
            auto xfOps = xformable.GetOrderedXformOps(&resetXformStack);
            for (auto& op : xfOps) {
                if (op.GetOpType() == UsdGeomXformOp::TypeTranslate) {
                    ops.translateOp = op;
                }
                if (op.GetOpType() == UsdGeomXformOp::TypeOrient) {
                    ops.orientOp = op;
                }
            }
            // Create ops if they don't exist yet
            if (!ops.translateOp) ops.translateOp = xformable.AddTranslateOp();
            if (!ops.orientOp) ops.orientOp = xformable.AddOrientOp();
            ops.valid = true;
        }
    }

    opsCache_[primPath] = ops;
    return opsCache_[primPath];
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
