#pragma once

#include <memory>
#include "core/compute_backend.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "stubs/stub_physics_process.h"
#include "stubs/stub_render_session.h"

namespace microbotica::stubs {

/// Local compute backend that creates stub physics and render instances.
class LocalComputeBackend : public core::ComputeBackend {
public:
    LocalComputeBackend() {
        MBCA_EXPERIMENTAL_WARN("LocalComputeBackend");
    }

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-002",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Local compute backend creating in-process stub instances.",
            .preconditions     = {},
            .postconditions    = {},
            .invariants        = {},
            .design_rationale  = "Provides the default compute backend for development and testing.",
            .assumptions       = {"All computation runs in-process on the local machine"},
            .limitations       = {"Uses stub physics — no real simulation"},
            .validated_regimes = {},
            .hazard_hints      = {},
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    std::unique_ptr<core::PhysicsProcess> createPhysicsProcess() override {
        return std::make_unique<StubPhysicsProcess>();
    }

    std::unique_ptr<core::RenderSession> createSession(const core::RenderConfig& /*config*/) override {
        return std::make_unique<StubRenderSession>();
    }

    std::string backendName() const override { return "local"; }
};

} // namespace microbotica::stubs
