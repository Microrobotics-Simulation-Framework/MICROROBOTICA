#pragma once

#include <string>
#include "component_meta.h"
#include "types.h"

namespace microbotica::core {

/// Abstract interface for a render session.
///
/// A render session is a connection to a running render backend
/// that provides viewport content.
///
/// MBCA-COMP-003
class RenderSession {
public:
    /// Return structured metadata for this interface.
    static const ComponentMeta& interfaceMeta() {
        static const ComponentMeta meta{
            .component_id      = "MBCA-COMP-003",
            .component_version = "1.0.0",
            .stability         = StabilityLevel::Experimental,
            .description       = "Abstract interface for render sessions.",
            .preconditions     = {"RenderBackend has been initialized"},
            .postconditions    = {"After requestSession(), a session URL is available or empty for local"},
            .invariants        = {},
            .design_rationale  = "Separates session lifecycle from backend initialization.",
            .assumptions       = {"Local sessions have empty URL; cloud sessions have WebRTC URL"},
            .limitations       = {"Cloud sessions (Selkies) are Phase 2"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Session disconnection during active simulation may cause stale viewport.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    virtual ~RenderSession() = default;

    /// @post Returns session URL (empty string for local rendering).
    virtual std::string requestSession() = 0;

    /// @return Current session status.
    virtual SessionStatus status() const = 0;

    /// @post Session is disconnected. status() returns Disconnected.
    virtual void disconnect() = 0;
};

} // namespace microbotica::core
