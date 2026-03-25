#pragma once

#include "core/component_meta.h"
#include "core/result_frame.h"
#include "core/physics_config.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdGeom/xformable.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

namespace microbotica::scene {

/// Applies ResultFrame data to the USD results layer.
///
/// MBCA-COMP-011
///
/// Maps ResultFrame::positions to xformOp:translate, orientations to
/// xformOp:orient, meshes to primvars:displayColor, and scalars to
/// custom attributes on the results layer.
class ResultsApplicator {
public:
#ifdef MICROBOTICA_HAS_USD
    ResultsApplicator(SdfLayerRefPtr resultsLayer, UsdStageRefPtr stage);
#endif

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-011",
            .component_version = "1.1.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Maps ResultFrame data to USD results layer (translate, orient, vertex colors).",
            .preconditions     = {"Results layer exists and is writable",
                                  "Stage contains the prim paths referenced in PhysicsConfig"},
            .postconditions    = {"Position values are written to xformOp:translate",
                                  "Orientation values are written to xformOp:orient",
                                  "Mesh vertex colors are written to primvars:displayColor",
                                  "Unknown prim paths trigger a warning log"},
            .invariants        = {"Base layer is never written to",
                                  "Override layer is never written to"},
            .design_rationale  = "Centralizes the ResultFrame→USD mapping to ensure "
                                 "data integrity through the rendering pipeline.",
            .assumptions       = {"actorToPrimPath mapping is consistent across frames"},
            .limitations       = {"Logs warning for unknown paths instead of raising error — see MBCA-ANO-001"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Silent data loss if prim paths change between frames — see MBCA-ANO-001.",
                "Partial write on crash — see MBCA-ANO-003.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Apply a ResultFrame to the results layer.
    void apply(const core::ResultFrame& frame,
               const core::PhysicsConfig& config);

private:
#ifdef MICROBOTICA_HAS_USD
    SdfLayerRefPtr resultsLayer_;
    UsdStageRefPtr stage_;
#endif
};

} // namespace microbotica::scene
