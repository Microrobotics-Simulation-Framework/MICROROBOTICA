// End to end against a real `mime serve` (MIME 0.3): opt-in, local only. Set MICROROBOTICA_LIVE_READY to the ready
// file of a `mime serve` already running A0 saved with its controller called back:
//
//   mime serve <a0_callback_saved> --commands tcp://127.0.0.1:0 --frames tcp://127.0.0.1:0 --ready-file ready.json
//
// (scripts/make_a0_fixtures.sh writes the folder). The test drives the session through MICROROBOTICA's own client and
// live PhysicsProcess: hello, three steps (another client's status answered while the first compiles), a live input,
// recording, running at real time, pause and stop, and checks each frame arrives, in order, mapped onto the stage
// binding. It stops the session.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#ifdef MICROBOTICA_HAS_ZMQ

#include "live/binding_frames.h"
#include "live/live_client.h"
#include "live/live_physics_process.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

using namespace microbotica::live;

TEST_CASE("A real mime serve: hello, step, set, record, run at real time, pause, stop", "[live][mime-serve]") {
    using Clock = std::chrono::steady_clock;
    const char* readyPath = std::getenv("MICROROBOTICA_LIVE_READY");
    if (readyPath == nullptr) {
        SKIP("MICROROBOTICA_LIVE_READY names no running mime serve's ready file");
    }
    std::ifstream in(readyPath);
    const ReadyFile ready = parseReadyFile(std::string(std::istreambuf_iterator<char>(in), {}));

    auto client = std::make_shared<LiveClient>(ready.commands, ready.frames);
    const Hello& hello = client->connect();
    REQUIRE(hello.controller.has_value());
    REQUIRE(hello.controller->takesLiveInputs());
    CHECK(hello.liveInputs.at(0).defaults() == std::vector<double>{0.0});
    REQUIRE(hello.report.report.has_value());
    CHECK_FALSE(hello.report.report->validity.empty());
    auto mapping = std::make_shared<const BindingFrames>(hello.binding, hello.schema, StageUnits{});
    CHECK(mapping->unbound().empty());

    LivePhysicsProcess process(client, mapping);
    process.setStartOnLaunch(false);
    std::vector<Reply> refusals;
    process.setReplyObserver([&](const Reply& r) {
        if (!r.ok) refusals.push_back(r);
    });
    process.launch(microbotica::core::PhysicsConfig{});

    // Three chunks now, from a session not running: frames 0 (the start) to 3. The first compiles the run's programs
    // (seconds); meanwhile another client's status is answered at once, saying so.
    Reply step;
    std::thread stepper([&] { step = client->request(stepRequest(3)); });
    LiveClient other(ready.commands, ready.frames, LiveClient::Timeouts::all(2000));
    double slowestStatus = 0.0;
    bool sawCompiling = false;
    const auto until = Clock::now() + std::chrono::seconds(20);
    while (!sawCompiling && Clock::now() < until) {
        const auto t0 = Clock::now();
        const Reply status = other.request(statusRequest());
        slowestStatus = std::max(slowestStatus, std::chrono::duration<double>(Clock::now() - t0).count());
        sawCompiling = status.status.compiling();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stepper.join();
    CHECK(sawCompiling);
    INFO("slowest status while compiling: " << slowestStatus << " s");
    CHECK(slowestStatus < 1.0);
    REQUIRE(step.ok);
    CHECK(step.status.step == static_cast<std::uint64_t>(3 * hello.chunkSteps));
    std::vector<microbotica::core::ResultFrame> frames;
    for (int i = 0; i < 4; ++i) {
        auto f = process.receiveResult();
        REQUIRE(f.has_value());
        frames.push_back(std::move(*f));
    }
    CHECK(frames[0].simTime == 0.0);
    CHECK(frames[3].simTime == Catch::Approx(3 * hello.chunkSteps * hello.dt));
    CHECK(frames[3].positions.count("/World/umr") == 1);
    CHECK(frames[3].scales.count("/World/rpm/magnetic/magnetic_torque_umr") == 1);

    REQUIRE(client->request(recordRequest(true)).ok);
    // Running at real time (pace 1): about 1 / (chunk * dt) frames a second, none dropped; the last 2 s of 3 (the
    // first chunk after start may still be compiling).
    const auto t0 = std::chrono::steady_clock::now();
    process.setPaused(false);  // start
    std::size_t running = 0;
    while (std::chrono::steady_clock::now() - t0 < std::chrono::seconds(3)) {
        auto f = process.receiveResult();
        REQUIRE(f.has_value());
        if (std::chrono::steady_clock::now() - t0 >= std::chrono::seconds(1)) {
            ++running;
        }
    }
    process.setPaused(true);  // pause
    const auto stats = client->stats();
    CHECK(stats.dropped == 0);
    CHECK(stats.outOfOrder == 0);
    CHECK(stats.decodeErrors == 0);
    const double perSecond = 1.0 / (static_cast<double>(hello.chunkSteps) * hello.dt);
    INFO("frames in the last 2 s: " << running << ", at real time " << 2 * perSecond);
    CHECK(running >= static_cast<std::size_t>(0.9 * 2 * perSecond));
    CHECK(running <= static_cast<std::size_t>(1.1 * 2 * perSecond) + 1);

    // A live input, and one the controller does not declare (refused in MIME's shape); a step acts on it.
    process.sendParameters({{hello.liveInputs.at(0).label, 0.002}});
    process.sendParameters({{"no_such_input", 1.0}});
    REQUIRE(refusals.size() == 1);
    CHECK(refusals[0].errors.at(0).code.rfind("live.", 0) == 0);
    const Reply after = client->request(stepRequest(1));
    REQUIRE(after.ok);

    process.stop();
    REQUIRE(process.runFolder().has_value());  // between chunks: the stop has taken effect, its folder written
    const std::filesystem::path folder(*process.runFolder());
    CHECK(std::filesystem::exists(folder / "report.json"));
    CHECK(std::filesystem::exists(folder / "commands.jsonl"));
    CHECK(std::filesystem::exists(folder / "recording.usdc"));
    CHECK(std::filesystem::exists(folder / "topology.json"));
}

#endif // MICROBOTICA_HAS_ZMQ
