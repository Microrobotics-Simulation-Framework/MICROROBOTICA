#pragma once

#include <atomic>
#include <thread>
#include "core/physics_process.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "core/threadsafe_queue.h"

namespace microbotica::mime {

/// Stub physics process emitting UMR confinement demo data.
///
/// MBCA-IMPL-012
///
/// Generates synthetic ResultFrames with positions, orientations,
/// and per-vertex mesh colors matching the UMR confinement demo scene
/// hierarchy (/World/Actors/UMR, /World/Actors/FieldArrow,
/// /World/Analysis/FlowField). For UI development without a MIME server.
class MimeStubPhysicsProcess : public core::PhysicsProcess {
public:
    MimeStubPhysicsProcess();
    ~MimeStubPhysicsProcess() override;

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-012",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Stub physics process emitting UMR confinement demo data "
                                 "with position, orientation, and vertex-color output.",
            .preconditions     = {},
            .postconditions    = {"Produces ResultFrames at ~2 Hz with monotonic simTime",
                                  "Each frame includes position + orientation for UMR and FieldArrow actors",
                                  "Each frame includes vertex colors for FlowField mesh"},
            .invariants        = {},
            .design_rationale  = "Enables end-to-end testing of the orientation and vertex-color "
                                 "pipeline without a running MIME server.",
            .assumptions       = {"actorToPrimPath contains 'rigid_body', 'ext_field', and 'flow_field'"},
            .limitations       = {"Data is purely synthetic — not physically meaningful",
                                  "FlowField uses 4 vertices (2x2 grid) for simplicity",
                                  "No parameter handling (sendParameters is a no-op)"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Synthetic data should never be presented as simulation results to users.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    void launch(const core::PhysicsConfig& config) override;
    std::optional<core::ResultFrame> receiveResult() override;
    void sendParameters(const nlohmann::json& params) override;
    void stop() override;
    core::ProcessStatus status() const override;

private:
    void workerLoop();

    core::PhysicsConfig config_;
    std::atomic<core::ProcessStatus> status_{core::ProcessStatus::Idle};
    std::atomic<bool> running_{false};
    std::thread worker_;
    core::ThreadSafeQueue<core::ResultFrame> queue_;
};

} // namespace microbotica::mime
