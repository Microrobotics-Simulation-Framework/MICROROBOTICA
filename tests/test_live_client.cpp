// The live client's transport and the live PhysicsProcess, against a stand-in mime serve (ZMQ REP and PUB) that
// answers with the replies and sends the frames captured from a real one (tests/fixtures/live/).

#include <catch2/catch_test_macros.hpp>

#ifdef MICROBOTICA_HAS_ZMQ

#include "live/binding_frames.h"
#include "live/live_client.h"
#include "live/live_physics_process.h"
#include "test_config.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <zmq.hpp>

using namespace microbotica::live;

namespace {

std::string readFixture(const std::string& name) {
    std::ifstream in(microbotica::test::fixturePath("live/" + name), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

/// A stand-in for `mime serve`: answers hello with A0's captured hello, set with a refusal for any label but
/// `offset`, stop with the captured stop, anything else with an accepting reply; publishes the captured frames 0, 1
/// and the last (number 5, final) once `start` arrives.
class StandIn {
public:
    StandIn(std::string commands, std::string frames) : commands_(std::move(commands)), frames_(std::move(frames)) {
        replies_ = nlohmann::json::parse(readFixture("replies_a0_callback.json"));
        hello_ = readFixture("hello_a0_callback.json");
        rep_ = std::thread([this] { serve(); });
        pub_ = std::thread([this] { publish(); });
    }
    ~StandIn() {
        running_ = false;
        rep_.join();
        pub_.join();
    }
    std::vector<std::string> received() {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

private:
    void serve() {
        zmq::context_t ctx(1);
        zmq::socket_t rep(ctx, zmq::socket_type::rep);
        rep.set(zmq::sockopt::linger, 0);
        rep.set(zmq::sockopt::rcvtimeo, 100);
        rep.bind(commands_);
        while (running_) {
            zmq::message_t in;
            if (!rep.recv(in, zmq::recv_flags::none)) continue;
            const auto request = nlohmann::json::parse(std::string(static_cast<char*>(in.data()), in.size()));
            const std::string command = request.value("command", "");
            {
                std::lock_guard<std::mutex> lock(mutex_);
                received_.push_back(command);
            }
            std::string answer;
            if (command == "hello") {
                answer = hello_;
            } else if (command == "set" && !request["values"].contains("offset")) {
                answer = replies_["set_refused"].dump();
            } else if (command == "stop") {
                answer = replies_["stop"].dump();
            } else {
                answer = nlohmann::json{{"protocol", "mime.live/1"}, {"ok", true}, {"command", command},
                                        {"state", command == "pause" ? "paused" : "running"}}.dump();
            }
            if (command == "start") {
                started_ = true;
            }
            zmq::message_t out(answer.size());
            std::memcpy(out.data(), answer.data(), answer.size());
            rep.send(out, zmq::send_flags::none);
        }
    }

    void publish() {
        zmq::context_t ctx(1);
        // An XPUB: it receives the subscriber's subscription, so the frames go out only once someone hears them.
        zmq::socket_t pub(ctx, zmq::socket_type::xpub);
        pub.set(zmq::sockopt::linger, 0);
        pub.set(zmq::sockopt::rcvtimeo, 100);
        pub.bind(frames_);
        bool subscribed = false;
        while (running_ && !(subscribed && started_)) {
            zmq::message_t subscription;
            if (pub.recv(subscription, zmq::recv_flags::none)) {
                subscribed = true;
            }
        }
        for (const char* name : {"frame_0.bin", "frame_1.bin", "frame_last.bin"}) {
            const std::string frame = readFixture(name);
            zmq::message_t out(frame.size());
            std::memcpy(out.data(), frame.data(), frame.size());
            pub.send(out, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::string commands_, frames_;
    nlohmann::json replies_;
    std::string hello_;
    std::atomic<bool> running_{true};
    std::atomic<bool> started_{false};
    std::mutex mutex_;
    std::vector<std::string> received_;
    std::thread rep_, pub_;
};

} // namespace

TEST_CASE("Live session: hello, start, frames through the binding, a refused set, stop with its run folder",
          "[integration][live]") {
    const std::string commands = "tcp://127.0.0.1:15585";
    const std::string frames = "tcp://127.0.0.1:15586";
    StandIn standIn(commands, frames);

    auto client = std::make_shared<LiveClient>(commands, frames, 3000);
    const Hello& hello = client->connect();
    CHECK(hello.experiment == "script_a0");
    auto mapping = std::make_shared<const BindingFrames>(hello.binding, hello.schema, StageUnits{});

    LivePhysicsProcess process(client, mapping);
    std::vector<Reply> replies;
    std::mutex repliesMutex;
    process.setReplyObserver([&](const Reply& r) {
        std::lock_guard<std::mutex> lock(repliesMutex);
        replies.push_back(r);
    });
    std::atomic<int> observed{0};
    process.setFrameObserver([&](const LiveFrame&) { ++observed; });

    process.launch(microbotica::core::PhysicsConfig{});
    REQUIRE(process.status() == microbotica::core::ProcessStatus::Running);

    std::vector<microbotica::core::ResultFrame> got;
    while (true) {
        auto frame = process.receiveResult();
        if (!frame) break;  // the final frame ends the stream
        got.push_back(std::move(*frame));
    }
    REQUIRE(got.size() == 3);
    CHECK(got[1].simTime > 0.0);
    CHECK(got[1].positions.count("/World/umr") == 1);
    CHECK(got[1].positions.count("/World/rpm/arm/link_6") == 1);
    CHECK(observed.load() == 3);
    const auto stats = client->stats();
    CHECK(stats.framesReceived == 3);
    CHECK(stats.dropped == 3);  // frames 2 to 4 were not sent: the gap is said
    CHECK(stats.finalSeen);

    process.sendParameters({{"gain", 1.0}});    // refused: no such live input
    process.sendParameters({{"offset", 0.002}});  // accepted
    process.stop();
    CHECK(process.runFolder().value_or("") == "/data/runs/script_a0-live");
    {
        std::lock_guard<std::mutex> lock(repliesMutex);
        REQUIRE(replies.size() == 4);  // start, set (refused), set, stop
        CHECK(replies[1].command == "set");
        CHECK_FALSE(replies[1].ok);
        CHECK(replies[1].errors.at(0).code == "live.input-unknown");
        CHECK(replies[2].ok);
    }
    CHECK(standIn.received() == std::vector<std::string>{"hello", "start", "set", "set", "stop"});
}

TEST_CASE("Live client: a request with no reply times out and the next one can still be sent", "[integration][live]") {
    // Nothing listens here: the request times out, and the client's REQ socket is replaced, not left stuck.
    LiveClient client("tcp://127.0.0.1:15595", "tcp://127.0.0.1:15596", 200);
    CHECK_THROWS_AS(client.request(statusRequest()), std::runtime_error);
    CHECK_THROWS_AS(client.request(statusRequest()), std::runtime_error);
    CHECK_THROWS_AS(client.connect(), std::runtime_error);
}

#endif // MICROBOTICA_HAS_ZMQ
