#pragma once

#include <memory>
#include <string>
#include "component_meta.h"
#include "physics_process.h"
#include "render_config.h"
#include "render_session.h"

namespace microbotica::core {

/// Abstract interface for compute backends.
///
/// A compute backend provides both physics process and render session
/// creation. Local backends create in-process instances; cloud backends
/// create remote instances via SkyPilot.
///
/// MBCA-COMP-004
class ComputeBackend {
public:
    /// Return structured metadata for this interface.
    static const ComponentMeta& interfaceMeta() {
        static const ComponentMeta meta{
            .component_id      = "MBCA-COMP-004",
            .component_version = "1.0.0",
            .stability         = StabilityLevel::Experimental,
            .description       = "Abstract interface for compute backends (local or cloud).",
            .preconditions     = {},
            .postconditions    = {"Created processes and sessions are owned by the caller"},
            .invariants        = {},
            .design_rationale  = "Unifies local and cloud compute behind a single factory interface.",
            .assumptions       = {"One compute backend is active at a time"},
            .limitations       = {"Cloud backends (SkyPilot) are Phase 2"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Backend selection affects data provenance chain — cloud backends "
                "introduce network latency and potential frame drops.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    virtual ~ComputeBackend() = default;

    /// @post Returns a new physics process owned by the caller.
    virtual std::unique_ptr<PhysicsProcess> createPhysicsProcess() = 0;

    /// @post Returns a new render session owned by the caller.
    virtual std::unique_ptr<RenderSession> createSession(const RenderConfig& config) = 0;

    /// @return Human-readable name of this backend.
    virtual std::string backendName() const = 0;
};

} // namespace microbotica::core
