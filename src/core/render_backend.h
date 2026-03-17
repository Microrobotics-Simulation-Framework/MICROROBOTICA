#pragma once

#include <memory>
#include <string>
#include "component_meta.h"
#include "render_config.h"
#include "render_session.h"

namespace microbotica::core {

/// Abstract interface for render backends.
///
/// Concrete implementations provide either local (Hydra/Storm)
/// or remote (Selkies) rendering.
///
/// MBCA-COMP-002
class RenderBackend {
public:
    /// Return structured metadata for this interface.
    static const ComponentMeta& interfaceMeta() {
        static const ComponentMeta meta{
            .component_id      = "MBCA-COMP-002",
            .component_version = "1.0.0",
            .stability         = StabilityLevel::Experimental,
            .description       = "Abstract interface for render backends.",
            .preconditions     = {"OpenGL 4.5 context available (local) or network connectivity (cloud)"},
            .postconditions    = {"After initialize(), backend is ready to create sessions"},
            .invariants        = {},
            .design_rationale  = "Allows swapping between local GPU rendering and cloud streaming.",
            .assumptions       = {"Exactly one render backend is active at a time"},
            .limitations       = {"Cloud streaming backend is Phase 2"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Rendering failures may cause stale viewport content to be displayed.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    virtual ~RenderBackend() = default;

    /// @post Backend is ready to create render sessions.
    /// @throws std::runtime_error if initialization fails.
    virtual void initialize() = 0;

    /// @pre initialize() has been called.
    /// @post Returns a new render session.
    virtual std::unique_ptr<RenderSession> createSession(const RenderConfig& config) = 0;

    /// @return Human-readable name of this backend.
    virtual std::string name() const = 0;
};

} // namespace microbotica::core
