#include <catch2/catch_test_macros.hpp>
#include <QCoreApplication>
#include <QTimer>
#include <QEventLoop>
#include <memory>

#include "simulation/simulation_controller.h"
#include "scene/scene_manager.h"
#include "stubs/local_compute_backend.h"
#include "core/physics_config.h"

using namespace microbotica::core;
using namespace microbotica::simulation;
using namespace microbotica::scene;
using namespace microbotica::stubs;

TEST_CASE("Integration: LocalComputeBackend -> SimulationController -> 10 frames",
          "[integration]") {
    // Qt requires QCoreApplication for signals/slots
    int argc = 0;
    char* argv[] = {nullptr};
    QCoreApplication app(argc, argv);

    SceneManager sceneMgr;
    SimulationController controller(sceneMgr);

    auto backend = std::make_unique<LocalComputeBackend>();
    controller.setComputeBackend(std::move(backend));

    PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Robot";

    int framesReceived = 0;
    QObject::connect(&controller,
        &SimulationController::frameReady,
        [&framesReceived](const ResultFrame& frame) {
            (void)frame;
            framesReceived++;
        });

    controller.launchPhysics(config);
    REQUIRE(controller.isRunning());

    // Poll frames via event loop + timer
    QTimer timer;
    QEventLoop loop;
    timer.setInterval(10); // 10ms poll interval

    QObject::connect(&timer, &QTimer::timeout, [&]() {
        controller.requestNextFrame();
        if (framesReceived >= 10) {
            timer.stop();
            loop.quit();
        }
    });

    // Safety timeout to prevent infinite hang
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(10000); // 10 second max
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        timer.stop();
        loop.quit();
    });

    timer.start();
    timeout.start();
    loop.exec();

    REQUIRE(framesReceived >= 10);
    REQUIRE(controller.currentTime() > 0.0);

    controller.stop();
    REQUIRE_FALSE(controller.isRunning());
}
