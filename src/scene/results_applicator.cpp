#include "scene/results_applicator.h"
#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/editContext.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/changeBlock.h>
#include <pxr/usd/sdf/attributeSpec.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
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

    // Write orientations as xformOp:orient (op order: translate first, orient second)
    for (const auto& [actor, quat] : frame.orientations) {
        auto it = config.actorToPrimPath.find(actor);
        if (it == config.actorToPrimPath.end()) {
            spdlog::warn("ResultsApplicator: No prim path mapping for actor '{}' — skipping orient", actor);
            continue;
        }

        const auto& primPath = it->second;
        auto prim = stage_->GetPrimAtPath(SdfPath(primPath));
        if (!prim.IsValid()) {
            spdlog::warn("ResultsApplicator: Unknown prim path '{}' for actor '{}' — orient data dropped",
                         primPath, actor);
            continue;
        }

        UsdGeomXformable xformable(prim);
        if (!xformable) {
            spdlog::warn("ResultsApplicator: Prim '{}' is not Xformable — cannot set orient",
                         primPath);
            continue;
        }

        bool resetXformStack = false;
        auto ops = xformable.GetOrderedXformOps(&resetXformStack);

        UsdGeomXformOp orientOp;
        for (auto& op : ops) {
            if (op.GetOpType() == UsdGeomXformOp::TypeOrient) {
                orientOp = op;
                break;
            }
        }
        if (!orientOp) {
            orientOp = xformable.AddOrientOp();
        }

        orientOp.Set(GfQuatd(quat.w, GfVec3d(quat.x, quat.y, quat.z)));
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

#else

// No-USD stub
void ResultsApplicator::apply(const core::ResultFrame& /*frame*/,
                               const core::PhysicsConfig& /*config*/)
{
    spdlog::debug("ResultsApplicator: USD not available, apply is a no-op");
}

#endif

} // namespace microbotica::scene
