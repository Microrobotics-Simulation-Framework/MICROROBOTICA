#include <catch2/catch_test_macros.hpp>
#include "stubs/stub_render_session.h"

using namespace microbotica::core;
using namespace microbotica::stubs;

TEST_CASE("StubRenderSession: requestSession returns empty", "[unit][stub-render]") {
    StubRenderSession session;
    REQUIRE(session.requestSession().empty());
}

TEST_CASE("StubRenderSession: status is Disconnected", "[unit][stub-render]") {
    StubRenderSession session;
    REQUIRE(session.status() == SessionStatus::Disconnected);
}

TEST_CASE("StubRenderSession: disconnect is no-op", "[unit][stub-render]") {
    StubRenderSession session;
    session.disconnect(); // Should not crash
    REQUIRE(session.status() == SessionStatus::Disconnected);
}
