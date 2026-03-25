#pragma once

#include <memory>
#include "core/compute_backend.h"
#include "core/component_meta.h"
#include "core/stability.h"
#include "connection/connection_config.h"
#include "connection/connection_manager.h"
#include "mime_adapter/mime_physics_process.h"
#include "stubs/stub_render_session.h"

namespace microbotica::mime {

/// Compute backend factory for MIME physics backends over ZMQ.
///
/// MBCA-IMPL-011
///
/// Takes a ConnectionConfig and uses ConnectionManager to resolve endpoints.
/// Creates MimePhysicsProcess instances connected to the resolved endpoints.
/// Render session is currently a stub (Phase D).
class MimeComputeBackend : public core::ComputeBackend {
public:
    explicit MimeComputeBackend(connection::ConnectionConfig config)
        : config_(std::move(config))
    {
        MBCA_EXPERIMENTAL_WARN("MimeComputeBackend");
    }

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-011",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Compute backend factory creating MimePhysicsProcess over ZMQ.",
            .preconditions     = {"ConnectionConfig is valid for the selected mode"},
            .postconditions    = {"createPhysicsProcess returns a MimePhysicsProcess with resolved endpoints"},
            .invariants        = {},
            .design_rationale  = "Separates endpoint resolution (ConnectionManager) from physics "
                                 "communication (MimePhysicsProcess) to allow independent testing.",
            .assumptions       = {"MIME backend is already running or will be started before launch()"},
            .limitations       = {"Render session is a stub — no remote rendering in Phase C",
                                  "No automatic MIME subprocess spawning (Phase D)"},
            .validated_regimes = {},
            .hazard_hints      = {
                "If ConnectionManager resolves to a stale endpoint (e.g., after spot preemption), "
                "MimePhysicsProcess will fail to connect or timeout after 10s.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    std::unique_ptr<core::PhysicsProcess> createPhysicsProcess() override {
        connection::ConnectionManager mgr;
        std::string pub_endpoint = mgr.resolveEndpoint(config_);

        // Derive REQ endpoint from PUB endpoint by replacing the port
        std::string req_endpoint = pub_endpoint;
        auto last_colon = req_endpoint.rfind(':');
        if (last_colon != std::string::npos) {
            req_endpoint = req_endpoint.substr(0, last_colon + 1)
                         + std::to_string(config_.zmq_req_port);
        }

        return std::make_unique<MimePhysicsProcess>(req_endpoint, pub_endpoint);
    }

    std::unique_ptr<core::RenderSession> createSession(const core::RenderConfig& /*config*/) override {
        return std::make_unique<stubs::StubRenderSession>();
    }

    std::string backendName() const override { return "mime"; }

private:
    connection::ConnectionConfig config_;
};

} // namespace microbotica::mime
