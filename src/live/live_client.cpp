#include "live/live_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <future>
#include <stdexcept>

#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#endif

namespace microbotica::live {

using Clock = std::chrono::steady_clock;

int LiveTimeouts::forCommand(const std::string& command) const {
    switch (replyWait(command)) {
        case ReplyWait::AtOnce: return atOnceMs;
        case ReplyWait::ChunkEnd: return chunkEndMs;
        case ReplyWait::RunFolder: return runFolderMs;
    }
    return chunkEndMs;
}

std::string LiveOutcome::errorText() const {
    if (!error) {
        return {};
    }
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "an unknown error";
    }
}

#ifdef MICROBOTICA_HAS_ZMQ
struct LiveClient::Sockets {
    zmq::context_t context{1};
    std::unique_ptr<zmq::socket_t> sub;  ///< made on the commands lane by connect, then the frames thread's
};
/// A lane's REQ socket, made and used on its thread alone.
struct LaneSocket {
    std::unique_ptr<zmq::socket_t> req;
};
#else
struct LiveClient::Sockets {};
struct LaneSocket {};
#endif

struct LiveClient::Job {
    nlohmann::json message;
    int timeoutMs = -1;
    bool connect = false;  ///< connect the frames socket first, then keep hello's reply
    Done done;
};

/// A worker thread taking requests one at a time, in the order posted, on a REQ socket of its own.
class LiveClient::Lane {
public:
    Lane(LiveClient& client, std::string name) : client_(client), name_(std::move(name)) {
        thread_ = std::thread([this] { loop(); });
    }
    ~Lane() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable()) {
            if (onThisThread()) {
                // The client's last reference dropped in one of this lane's callbacks, which LiveClient forbids.
                spdlog::critical("LiveClient: destroyed on its own {} lane's thread", name_);
                thread_.detach();
            } else {
                thread_.join();
            }
        }
    }
    void post(Job job) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(std::move(job));
        }
        cv_.notify_one();
    }
    bool onThisThread() const { return std::this_thread::get_id() == thread_.get_id(); }

private:
    void loop() {
        LaneSocket socket;
        for (;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                if (jobs_.empty()) {
                    break;  // stopping, and every job posted has had its outcome
                }
                job = std::move(jobs_.front());
                jobs_.pop_front();
            }
            Outcome outcome;
            outcome.request = job.message;
            try {
                client_.run(job, outcome, &socket);
            } catch (...) {
                outcome.reply.reset();
                outcome.error = std::current_exception();
            }
            if (job.done) {
                try {
                    job.done(outcome);
                } catch (const std::exception& e) {
                    spdlog::error("LiveClient: a {} callback threw: {}", name_, e.what());
                }
            }
        }
    }

    LiveClient& client_;
    std::string name_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    bool stopping_ = false;
};

LiveClient::LiveClient(std::string commands, std::string frames, Timeouts timeouts, Security security)
    : commands_(std::move(commands)), frames_(std::move(frames)), timeouts_(timeouts), security_(std::move(security)),
      sockets_(std::make_unique<Sockets>())
{
    commandsLane_ = std::make_unique<Lane>(*this, "commands");
    statusLane_ = std::make_unique<Lane>(*this, "status");
}

LiveClient::~LiveClient() {
    close();  // every request in flight ends within 100 ms; those still queued fail at once
    commandsLane_.reset();
    statusLane_.reset();
}

bool LiveClient::connected() const {
    std::lock_guard<std::mutex> lock(helloMutex_);
    return hello_.has_value();
}

const Hello& LiveClient::hello() const {
    std::lock_guard<std::mutex> lock(helloMutex_);
    if (!hello_) {
        throw std::logic_error("LiveClient::hello() before connect()");
    }
    return *hello_;  // set once, never changed after
}

void LiveClient::connectAsync(Done done) {
    Job job;
    job.message = helloRequest();
    job.connect = true;
    job.done = std::move(done);
    commandsLane_->post(std::move(job));
}

const Hello& LiveClient::connect() {
    if (commandsLane_->onThisThread()) {
        throw std::logic_error("LiveClient::connect() from a commands-lane callback would wait for itself");
    }
    std::promise<void> connected;
    auto future = connected.get_future();
    connectAsync([&connected](const Outcome& o) {
        if (o.error) {
            connected.set_exception(o.error);
        } else {
            connected.set_value();
        }
    });
    future.get();
    return hello();
}

void LiveClient::post(const nlohmann::json& message, Done done, int timeoutMs) {
    Job job;
    job.message = message;
    job.timeoutMs = timeoutMs;
    job.done = std::move(done);
    commandsLane_->post(std::move(job));
}

void LiveClient::postStatus(Done done) {
    Job job;
    job.message = statusRequest();
    job.done = std::move(done);
    statusLane_->post(std::move(job));
}

Reply LiveClient::request(const nlohmann::json& message, int timeoutMs) {
    if (commandsLane_->onThisThread()) {
        throw std::logic_error("LiveClient::request() from a commands-lane callback would wait for itself");
    }
    std::promise<Reply> replied;
    auto future = replied.get_future();
    post(message, [&replied](const Outcome& o) {
        if (o.error) {
            replied.set_exception(o.error);
        } else {
            replied.set_value(*o.reply);
        }
    }, timeoutMs);
    return future.get();
}

#ifdef MICROBOTICA_HAS_ZMQ

/// Watches a socket for its first completed handshake (ZMTP's greeting and READY both ways).
class HandshakeWatch : public zmq::monitor_t {
public:
    bool done = false;
    void on_event_handshake_succeeded(const zmq_event_t&, const char*) override { done = true; }
};

/// Watches the frames' socket for its connection closing: the server has gone (after a stop, or a crash).
class DisconnectWatch : public zmq::monitor_t {
public:
    bool gone = false;
    void on_event_disconnected(const zmq_event_t&, const char*) override { gone = true; }
};

namespace {

std::atomic<int> monitors{0};  // each monitor's inproc endpoint is its own

/// The next wait: at most 100 ms, so that close() is seen, and never past the deadline.
int slice(Clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    return static_cast<int>(std::clamp<long long>(left, 0, 100));
}

} // namespace

void LiveClient::connectFrames() {
    if (sockets_->sub) {
        return;
    }
    sockets_->sub = std::make_unique<zmq::socket_t>(sockets_->context, zmq::socket_type::sub);
    sockets_->sub->set(zmq::sockopt::linger, 0);
    sockets_->sub->set(zmq::sockopt::rcvtimeo, 200);
    sockets_->sub->set(zmq::sockopt::rcvhwm, 10000);
    sockets_->sub->set(zmq::sockopt::subscribe, "");
    // The frames socket's connection is made in the background. Wait (a little) for its handshake before asking
    // hello, so a step sent right after connecting does not lose its frames to a subscription still on its way (ZMQ's
    // "slow joiner").
    HandshakeWatch watch;
    watch.init(*sockets_->sub, "inproc://microrobotica-live-sub-" + std::to_string(++monitors),
               ZMQ_EVENT_HANDSHAKE_SUCCEEDED);
    sockets_->sub->connect(frames_);
    const auto deadline = Clock::now() + std::chrono::milliseconds(2000);
    while (!watch.done && Clock::now() < deadline && !closed_.load()) {
        watch.check_event(50);
    }
    if (!watch.done && !closed_.load()) {
        spdlog::warn("LiveClient: no handshake on the frames socket {} within 2 s; the first frames may be lost",
                     frames_);
    }
}

void LiveClient::run(Job& job, Outcome& outcome, void* laneSocket) {
    auto& socket = static_cast<LaneSocket*>(laneSocket)->req;
    const std::string command = job.message.value("command", std::string("request"));
    const std::string what = "mime.live/1 " + command + " to " + commands_;
    if (closed_.load()) {
        throw std::runtime_error(what + ": the session is closed");
    }
    if (const auto refusal = mechanismRefusal(security_)) {
        throw ProtocolError(what + ": " + *refusal);  // before any socket: never a null-mechanism fallback
    }
    if (job.connect) {
        connectFrames();
    }
    const int timeout = job.timeoutMs >= 0 ? job.timeoutMs : timeouts_.forCommand(command);
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout);
    // A REQ socket whose reply never came cannot send again: the next request makes a fresh one.
    auto fail = [&](const std::string& why) {
        socket.reset();
        throw std::runtime_error(what + ": " + why);
    };

    try {
        if (!socket) {
            socket = std::make_unique<zmq::socket_t>(sockets_->context, zmq::socket_type::req);
            socket->set(zmq::sockopt::linger, 0);
            socket->connect(commands_);
        }
        // Send, then wait for the reply, in slices of at most 100 ms, so that close() ends the wait.
        for (;;) {
            zmq_pollitem_t item{socket->handle(), 0, ZMQ_POLLOUT, 0};
            if (zmq_poll(&item, 1, slice(deadline)) > 0 && (item.revents & ZMQ_POLLOUT)) {
                break;
            }
            if (closed_.load()) fail("the session is closed");
            if (Clock::now() >= deadline) {
                fail("the request could not be sent within " + std::to_string(timeout) + " ms");
            }
        }
        const std::string body = job.message.dump();
        zmq::message_t out(body.size());
        std::memcpy(out.data(), body.data(), body.size());
        if (!socket->send(out, zmq::send_flags::dontwait)) {
            fail("the request could not be sent");
        }
        for (;;) {
            zmq_pollitem_t item{socket->handle(), 0, ZMQ_POLLIN, 0};
            if (zmq_poll(&item, 1, slice(deadline)) > 0 && (item.revents & ZMQ_POLLIN)) {
                zmq::message_t in;
                if (socket->recv(in, zmq::recv_flags::dontwait)) {
                    outcome.reply = parseReply(std::string(static_cast<const char*>(in.data()), in.size()));
                    break;
                }
            }
            if (closed_.load()) fail("the session is closed");
            if (Clock::now() >= deadline) fail("no reply within " + std::to_string(timeout) + " ms");
        }
    } catch (const zmq::error_t& e) {
        fail(e.what());
    }

    if (!outcome.reply->command.empty() && outcome.reply->command != command) {
        // Another client's reply (a MIME before PR #89's fix round could send one): this one's is lost.
        throw ProtocolError(what + ": the reply is to " + outcome.reply->command + ", not to " + command);
    }
    if (job.connect) {
        Hello parsed = parseHello(outcome.reply->raw);
        spdlog::info("LiveClient: {} at {} (MIME {}): {} fields a frame ({} bytes), {} bound prims, {} live input(s)",
                     parsed.experiment, commands_, parsed.mimeVersion, parsed.schema.fields.size(),
                     parsed.schema.frameBytes, parsed.binding.size(), parsed.liveInputs.size());
        std::lock_guard<std::mutex> lock(helloMutex_);
        hello_ = std::move(parsed);
    }
}

void LiveClient::startFrames(std::function<void(LiveFrame)> sink) {
    if (!connected()) {
        throw std::logic_error("LiveClient::startFrames() before connect()");
    }
    if (receiving_.exchange(true)) {
        return;
    }
    frameThread_ = std::thread(&LiveClient::frameLoop, this, std::move(sink));
}

void LiveClient::stopFrames() {
    receiving_.store(false);
    if (frameThread_.joinable()) {
        frameThread_.join();
    }
}

void LiveClient::frameLoop(std::function<void(LiveFrame)> sink) {
    FrameOrder order;
    const FrameSchema schema = hello().schema;
    DisconnectWatch closing;
    closing.init(*sockets_->sub, "inproc://microrobotica-live-sub-" + std::to_string(++monitors),
                 ZMQ_EVENT_DISCONNECTED);
    while (receiving_.load()) {
        while (!closing.gone && closing.check_event(0)) {
        }
        if (closing.gone) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            if (!stats_.disconnected) {
                stats_.disconnected = true;
                ended_.notify_all();
            }
        }
        zmq::message_t message;
        zmq::recv_result_t got;
        try {
            got = sockets_->sub->recv(message, zmq::recv_flags::none);
        } catch (const zmq::error_t& e) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.lastProblem = std::string("frames socket: ") + e.what();
            break;
        }
        if (!got) {
            continue;  // the 200 ms receive timeout: look again at whether to stop
        }
        const auto* data = static_cast<const std::uint8_t*>(message.data());
        const std::size_t size = message.size();
        const auto t0 = Clock::now();
        try {
            LiveFrame frame = decodeFrame(data, size, schema);
            const double us = std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
            const std::string problem = order.see(frame.number);
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                ++stats_.framesReceived;
                stats_.bytesReceived += size;
                stats_.lastFrameBytes = static_cast<std::uint32_t>(size);
                stats_.decodeAvgUs += (us - stats_.decodeAvgUs) / static_cast<double>(stats_.framesReceived);
                stats_.decodeMaxUs = std::max(stats_.decodeMaxUs, us);
                stats_.dropped = order.dropped();
                stats_.outOfOrder = order.outOfOrder();
                if (!problem.empty()) {
                    stats_.lastProblem = problem;
                }
                if (frame.final) {
                    stats_.finalSeen = true;
                    ended_.notify_all();
                }
            }
            if (!problem.empty()) {
                spdlog::warn("LiveClient: {}", problem);
            }
            sink(std::move(frame));
        } catch (const ProtocolError& e) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            ++stats_.decodeErrors;
            stats_.lastProblem = e.what();
            spdlog::warn("LiveClient: a frame not read: {}", e.what());
        }
    }
}

#else  // no ZMQ: the client cannot connect

void LiveClient::connectFrames() {}

void LiveClient::run(Job&, Outcome&, void*) {
    throw std::runtime_error("this build has no ZMQ: it cannot reach a mime.live/1 session");
}

void LiveClient::startFrames(std::function<void(LiveFrame)>) {}

void LiveClient::stopFrames() {}

void LiveClient::frameLoop(std::function<void(LiveFrame)>) {}

#endif

void LiveClient::close() {
    closed_.store(true);
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        ended_.notify_all();
    }
    stopFrames();
}

LiveClient::Stats LiveClient::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

bool LiveClient::waitForEnd(int timeoutMs) {
    std::unique_lock<std::mutex> lock(statsMutex_);
    return ended_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                           [this] { return stats_.finalSeen || stats_.disconnected || closed_.load(); });
}

} // namespace microbotica::live
