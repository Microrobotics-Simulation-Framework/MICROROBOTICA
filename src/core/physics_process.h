#pragma once

#include <optional>
#include <string>
#include <nlohmann/json.hpp>
#include "component_meta.h"
#include "types.h"
#include "result_frame.h"
#include "physics_config.h"

namespace microbotica::core {

/// Abstract interface for a physics simulation backend.
///
/// Concrete implementations communicate with a physics engine
/// (MIME/MADDENING via ZeroMQ, or a stub for testing).
///
/// MBCA-COMP-001
class PhysicsProcess {
public:
    /// Return structured metadata for this interface.
    static const ComponentMeta& interfaceMeta() {
        static const ComponentMeta meta{
            .component_id      = "MBCA-COMP-001",
            .component_version = "1.0.0",
            .stability         = StabilityLevel::Experimental,
            .description       = "Abstract interface for physics simulation backends.",
            .preconditions     = {"Backend binary must be available on PATH or via IPC endpoint"},
            .postconditions    = {"After launch(), status() returns Running or Error",
                                  "After stop(), status() returns Stopped"},
            .invariants        = {},
            .design_rationale  = "Decouples MICROBOTICA from any specific physics engine. "
                                 "Enables stub testing and future cloud offloading.",
            .assumptions       = {"Backend communicates via IPC (ZeroMQ) or in-process calls",
                                  "ResultFrame data is serializable as JSON"},
            .limitations       = {"receiveResult() blocks indefinitely — see MBCA-ANO-004",
                                  "No timeout or cancellation mechanism in Phase 0"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Backend hang causes indefinite blocking — see MBCA-ANO-004.",
                "No validation of ResultFrame values (NaN, Inf, out-of-range) at interface level.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    virtual ~PhysicsProcess() = default;

    /// @pre PhysicsConfig has valid actorToPrimPath entries.
    /// @post status() returns Running or Error.
    /// @throws std::runtime_error if launch fails.
    virtual void launch(const PhysicsConfig& config) = 0;

    /// @pre status() == Running.
    /// @post Returns a ResultFrame or std::nullopt on stream end.
    /// @note Blocks until data is available. See MBCA-ANO-004.
    virtual std::optional<ResultFrame> receiveResult() = 0;

    /// @pre status() == Running.
    /// @post Parameters are forwarded to the backend.
    /// @throws std::runtime_error if backend rejects parameters.
    virtual void sendParameters(const nlohmann::json& params) = 0;

    /// @pre status() == Running.
    /// @post status() returns Stopped.
    virtual void stop() = 0;

    /// @return Current process status.
    virtual ProcessStatus status() const = 0;
};

} // namespace microbotica::core
