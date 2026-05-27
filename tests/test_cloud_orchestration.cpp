// Coverage for the bits of the Cloud-mode launch path that don't need a
// running Qt event loop / live SkyPilot cluster — argument construction,
// the port probe primitive, and the basic invariants of the CloudJobLauncher
// state machine. The end-to-end (sky launch → tunnel → handshake) flow is
// covered manually against a real cluster; this file is the regression net
// for the pieces that are pure functions or self-contained classes.

#include <catch2/catch_test_macros.hpp>

#include <QStringList>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>

#include "experiment/cloud_job_launcher.h"
#include "experiment/experiment_runner.h"
#include "mime_adapter/mime_physics_process.h"

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#endif

using microbotica::experiment::CloudJobLauncher;
using microbotica::experiment::ExperimentRunner;
using microbotica::mime::MimePhysicsProcess;

// ── CloudJobLauncher::buildLaunchArgs ───────────────────────────────────────

TEST_CASE("CloudJobLauncher::buildLaunchArgs forms the expected sky launch invocation",
          "[unit][cloud]") {
    const QStringList args = CloudJobLauncher::buildLaunchArgs(
        "/home/nick/MSF/msf/MIME/experiments/ar4_helical_drive",
        "jobs/local_ssh.yaml",
        "ar4-test");

    // Must be `sky launch` (not `sky jobs launch`) — interactive cluster.
    REQUIRE(args.value(0) == QLatin1String("launch"));
    REQUIRE_FALSE(args.contains(QLatin1String("jobs")));

    // Non-interactive and detached so the launcher returns once the task
    // is dispatched, leaving the cluster up for us to tunnel into.
    REQUIRE(args.contains(QLatin1String("-y")));
    REQUIRE(args.contains(QLatin1String("-d")));

    // Cluster name + workdir wired into the right flags.
    const int cIdx = args.indexOf(QLatin1String("-c"));
    REQUIRE(cIdx >= 0);
    REQUIRE(args.value(cIdx + 1) == QLatin1String("ar4-test"));

    const int wIdx = args.indexOf(QLatin1String("--workdir"));
    REQUIRE(wIdx >= 0);
    REQUIRE(args.value(wIdx + 1) ==
            QLatin1String("/home/nick/MSF/msf/MIME/experiments/ar4_helical_drive"));

    // Job YAML path is `experiment_dir + "/" + job_config`.
    REQUIRE(args.last() ==
            QLatin1String(
                "/home/nick/MSF/msf/MIME/experiments/ar4_helical_drive/"
                "jobs/local_ssh.yaml"));
}

TEST_CASE("CloudJobLauncher::buildLaunchArgs canonicalizes workdir through symlinks",
          "[unit][cloud]") {
    // When MICROROBOTICA runs in the cloud-dev container, the user may open
    // an experiment via a /workspace/<x> symlink while the sky API server
    // (on the host) resolves --workdir against host-real paths. The
    // canonical() step inside buildLaunchArgs must follow the symlink so
    // sky gets a path the host can stat.
    namespace fs = std::filesystem;
    const auto tmp = fs::temp_directory_path() /
        ("microbotica-cloud-test-" + std::to_string(::getpid()));
    const auto real_dir = tmp / "real";
    const auto link_dir = tmp / "link";
    fs::create_directories(real_dir);
    fs::create_directory_symlink(real_dir, link_dir);

    const QStringList args = CloudJobLauncher::buildLaunchArgs(
        link_dir.string(),     // user-facing path (symlink)
        "jobs/x.yaml",
        "test-cluster");

    const int wIdx = args.indexOf(QLatin1String("--workdir"));
    REQUIRE(wIdx >= 0);
    REQUIRE(args.value(wIdx + 1) ==
            QString::fromStdString(real_dir.string()));  // resolved through link
    REQUIRE(args.last() ==
            QString::fromStdString(real_dir.string() + "/jobs/x.yaml"));

    fs::remove_all(tmp);
}

TEST_CASE("CloudJobLauncher: idle state has no process",
          "[unit][cloud]") {
    CloudJobLauncher launcher;
    REQUIRE_FALSE(launcher.isLaunching());
    // cancel() on an idle launcher should be safe (spawns `sky down` and
    // detaches; we just verify it doesn't crash on no cluster).
    // Note: the actual `sky` invocation may fail if sky isn't installed —
    // we deliberately do not assert on side effects of the spawn.
    launcher.cancel("does-not-exist");
}

// ── ExperimentRunner::probePort (static) ────────────────────────────────────

TEST_CASE("ExperimentRunner::probePort returns false on a closed port",
          "[unit][cloud]") {
    // 59998 is in the ephemeral range and overwhelmingly likely to be free.
    REQUIRE_FALSE(ExperimentRunner::probePort(59998, 200));
}

#ifdef MICROBOTICA_HAS_ZMQ
TEST_CASE("ExperimentRunner::probePort accepts when something is listening",
          "[integration][cloud]") {
    // Use a ZMQ socket as a convenient way to bind a port from C++.
    const int port = 16001;
    std::atomic<bool> running{true};
    std::thread listener([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 200);
        rep.bind("tcp://127.0.0.1:" + std::to_string(port));
        while (running.load()) {
            zmq::message_t msg;
            (void)rep.recv(msg, zmq::recv_flags::none);
        }
    });

    // Let the bind settle.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(ExperimentRunner::probePort(port, 500));

    running.store(false);
    listener.join();
}

// ── MimePhysicsProcess::probeRunner ─────────────────────────────────────────
//
// This is the ZMQ-level readiness probe the Cloud-mode flow uses *after*
// the SSH tunnel is up — sky launch -d returns once the job is dispatched,
// not when the runner has bound. The TCP-only check was a false positive
// because ssh -L accepts locally regardless of the upstream state.

TEST_CASE("MimePhysicsProcess::probeRunner returns false when nothing answers",
          "[unit][cloud]") {
    // 59999 deliberately closed — confirms the helper does not deadlock or
    // return spuriously when the upstream is absent.
    REQUIRE_FALSE(
        MimePhysicsProcess::probeRunner("tcp://127.0.0.1:59999", 200));
}

TEST_CASE("MimePhysicsProcess::probeRunner returns true on any REP reply",
          "[integration][cloud]") {
    const int port = 16002;
    std::atomic<bool> running{true};
    std::thread listener([&]() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 500);
        rep.bind("tcp://127.0.0.1:" + std::to_string(port));
        while (running.load()) {
            zmq::message_t msg;
            auto r = rep.recv(msg, zmq::recv_flags::none);
            if (r) {
                // Any non-empty reply counts as "runner up" — the probe
                // does not parse the body, just verifies a round-trip.
                const std::string body = R"({"stream_info":"stub"})";
                zmq::message_t out(body.size());
                std::memcpy(out.data(), body.data(), body.size());
                (void)rep.send(out, zmq::send_flags::none);
            }
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    REQUIRE(MimePhysicsProcess::probeRunner(
        "tcp://127.0.0.1:" + std::to_string(port), 1000));

    running.store(false);
    listener.join();
}
#endif // MICROBOTICA_HAS_ZMQ
