#pragma once

#include "core/render_session.h"
#include "core/stability.h"

namespace microbotica::stubs {

/// Stub render session that is always disconnected.
class StubRenderSession : public core::RenderSession {
public:
    StubRenderSession() {
        MBCA_EXPERIMENTAL_WARN("StubRenderSession");
    }

    std::string requestSession() override { return ""; }
    core::SessionStatus status() const override { return core::SessionStatus::Disconnected; }
    void disconnect() override {}
};

} // namespace microbotica::stubs
