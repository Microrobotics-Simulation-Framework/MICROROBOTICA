#pragma once

#include <atomic>
#include <thread>
#include "core/physics_process.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "core/threadsafe_queue.h"

namespace microbotica::stubs {

/// Stub physics process that generates synthetic sinusoidal data.
///
/// MBCA-IMPL-001
class StubPhysicsProcess : public core::PhysicsProcess {
public:
    StubPhysicsProcess();
    ~StubPhysicsProcess() override;

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-001",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Stub physics process producing synthetic sinusoidal data for testing.",
            .preconditions     = {},
            .postconditions    = {"Produces ResultFrames at ~60 Hz with monotonic simTime",
                                  "Each frame includes position and orientation per actor"},
            .invariants        = {},
            .design_rationale  = "Allows end-to-end testing of the simulation pipeline "
                                 "without a real physics backend.",
            .assumptions       = {"No real physics computation; data is purely synthetic"},
            .limitations       = {"Positions follow sin(t) — not physically meaningful",
                                  "Orientations rotate around Z at 1 rad/s — not physically meaningful",
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
    void setPaused(bool paused) override;
    core::ProcessStatus status() const override;

private:
    void workerLoop();

    core::PhysicsConfig config_;
    std::atomic<core::ProcessStatus> status_{core::ProcessStatus::Idle};
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::thread worker_;
    core::ThreadSafeQueue<core::ResultFrame> queue_;
};

} // namespace microbotica::stubs
