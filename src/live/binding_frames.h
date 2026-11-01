#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/physics_config.h"
#include "core/result_frame.h"
#include "live/live_protocol.h"

namespace microbotica::live {

/// What the stage says of its units, which the mapping needs: MIME's frames are in SI (metres), and a stage may be
/// written in another length unit (millimetres: metersPerUnit 0.001).
struct StageUnits {
    /// The stage's metersPerUnit (UsdGeomGetStageMetersPerUnit): every position is divided by it.
    double metersPerUnit = 1.0;
    /// Each vector prim's `mime:vector:metres_per_unit`: its arrow's length in metres per unit of its field.
    std::unordered_map<std::string, double> vectorMetresPerUnit;
};

/// The stage binding applied to frames: each frame's bound fields as the prims' transforms, written as a ResultFrame
/// keyed by prim path, so the scene's ResultsApplicator writes them to the results layer.
///
/// - **pose** (x, y, z, then w, x, y, z): the prim's translate (the position divided by the stage's metersPerUnit) and
///   orient, as they stand. MIME places every bound prim in the world (a chain's frame prims reset their transform
///   stack), so no composition with a parent.
/// - **vector** (three numbers in the world's axes): its arrow at the position of the pose it is drawn at, turned from
///   +Z onto the vector, scaled uniformly to |v| x its metres per unit, and made visible (the stage hides it), as
///   MIME's recordings draw it.
/// - Every scalar field of the schema (an observation's output, a flag) as a scalar by its path, for the plots.
class BindingFrames {
public:
    BindingFrames(std::vector<BindingEntry> binding, const FrameSchema& schema, StageUnits units);

    /// One frame as the prims' transforms and the frame's scalars. Entries whose field the frame lacks are skipped.
    core::ResultFrame toResultFrame(const LiveFrame& frame) const;

    /// Every bound prim, mapped to itself (the ResultFrame is keyed by prim path).
    core::PhysicsConfig physicsConfig() const;

    /// Binding entries whose field the schema does not carry (they cannot move): "<prim> (<node>.<field>)".
    const std::vector<std::string>& unbound() const { return unbound_; }

    const std::vector<BindingEntry>& entries() const { return binding_; }

    /// The unit quaternion (w, x, y, z) turning +Z onto `v`, the shorter way; identity for a zero vector.
    static core::Quatf turnFromZ(double x, double y, double z);

private:
    std::vector<BindingEntry> binding_;
    std::vector<std::string> scalarPaths_;
    StageUnits units_;
    std::vector<std::string> unbound_;
};

} // namespace microbotica::live
