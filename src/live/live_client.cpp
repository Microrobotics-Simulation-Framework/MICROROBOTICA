#include "live/live_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>

#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_ZMQ
#include <zmq.hpp>
#endif

namespace microbotica::live {

#ifdef MICROBOTICA_HAS_ZMQ
struct LiveClient::Sockets {
    zmq::context_t context{1};
    std::unique_ptr<zmq::socket_t> req;
    std::unique_ptr<zmq::socket_t> sub;
};
#else
struct LiveClient::Sockets {};
#endif

LiveClient::LiveClient(std::string commands, std::string frames, int timeoutMs)
    : commands_(std::move(commands)), frames_(std::move(frames)), timeoutMs_(timeoutMs),
      sockets_(std::make_unique<Sockets>())
{
}

LiveClient::~LiveClient() {
    stopFrames();
}

const Hello& LiveClient::hello() const {
    if (!hello_) {
        throw std::logic_error("LiveClient::hello() before connect()");
    }
    return *hello_;
}

#ifdef MICROBOTICA_HAS_ZMQ

namespace {

std::unique_ptr<zmq::socket_t> makeReq(zmq::context_t& context, const std::string& endpoint, int timeoutMs) {
    auto req = std::make_unique<zmq::socket_t>(context, zmq::socket_type::req);
    req->set(zmq::sockopt::linger, 0);
    req->set(zmq::sockopt::rcvtimeo, timeoutMs);
    req->set(zmq::sockopt::sndtimeo, timeoutMs);
    req->connect(endpoint);
    return req;
}

} // namespace

/// Watches a socket for its first completed handshake (ZMTP's greeting and READY both ways).
class HandshakeWatch : public zmq::monitor_t {
public:
    bool done = false;
    void on_event_handshake_succeeded(const zmq_event_t&, const char*) override { done = true; }
};

const Hello& LiveClient::connect() {
    if (!sockets_->sub) {
        sockets_->sub = std::make_unique<zmq::socket_t>(sockets_->context, zmq::socket_type::sub);
        sockets_->sub->set(zmq::sockopt::linger, 0);
        sockets_->sub->set(zmq::sockopt::rcvtimeo, 200);
        sockets_->sub->set(zmq::sockopt::rcvhwm, 10000);
        sockets_->sub->set(zmq::sockopt::subscribe, "");
        // The frames socket's connection is made in the background. Wait (a
        // little) for its handshake before asking hello, so a step sent right
        // after connect() does not lose its frames to a subscription still
        // on its way (ZMQ's "slow joiner").
        HandshakeWatch watch;
        static std::atomic<int> watches{0};
        watch.init(*sockets_->sub, "inproc://microrobotica-live-sub-" + std::to_string(++watches),
                   ZMQ_EVENT_HANDSHAKE_SUCCEEDED);
        sockets_->sub->connect(frames_);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        while (!watch.done && std::chrono::steady_clock::now() < deadline) {
            watch.check_event(50);
        }
        if (!watch.done) {
            spdlog::warn("LiveClient: no handshake on the frames socket {} within 2 s; the first frames may be lost",
                         frames_);
        }
    }
    const Reply reply = request(helloRequest());
    hello_ = parseHello(reply.raw);
    spdlog::info("LiveClient: {} at {} (MIME {}): {} fields a frame ({} bytes), {} bound prims, {} live input(s)",
                 hello_->experiment, commands_, hello_->mimeVersion, hello_->schema.fields.size(),
                 hello_->schema.frameBytes, hello_->binding.size(), hello_->liveInputs.size());
    return *hello_;
}

Reply LiveClient::request(const nlohmann::json& message, int timeoutMs) {
    if (closed_.load()) {
        throw std::runtime_error("mime.live/1 " + message.value("command", std::string("request")) +
                                 ": the session is closed");
    }
    const int timeout = timeoutMs >= 0 ? timeoutMs : timeoutMs_;
    const std::string body = message.dump();
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (!sockets_->req) {
        sockets_->req = makeReq(sockets_->context, commands_, timeout);
    }
    sockets_->req->set(zmq::sockopt::rcvtimeo, timeout);
    sockets_->req->set(zmq::sockopt::sndtimeo, timeout);
    std::string failure;
    try {
        zmq::message_t out(body.size());
        std::memcpy(out.data(), body.data(), body.size());
        if (!sockets_->req->send(out, zmq::send_flags::none)) {
            failure = "the request could not be sent within " + std::to_string(timeout) + " ms";
        } else {
            zmq::message_t in;
            if (sockets_->req->recv(in, zmq::recv_flags::none)) {
                return parseReply(std::string(static_cast<const char*>(in.data()), in.size()));
            }
            failure = "no reply within " + std::to_string(timeout) + " ms";
        }
    } catch (const zmq::error_t& e) {
        failure = e.what();
    }
    // A REQ socket whose reply never came cannot send again: start a fresh one for the next request.
    sockets_->req.reset();
    throw std::runtime_error("mime.live/1 " + message.value("command", std::string("request")) + " to " + commands_ +
                             ": " + failure);
}

void LiveClient::startFrames(std::function<void(LiveFrame)> sink) {
    if (!hello_) {
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
    const FrameSchema& schema = hello_->schema;
    while (receiving_.load()) {
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
        const auto t0 = std::chrono::steady_clock::now();
        try {
            LiveFrame frame = decodeFrame(data, size, schema);
            const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
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

const Hello& LiveClient::connect() {
    throw std::runtime_error("this build has no ZMQ: it cannot reach a mime.live/1 session");
}

Reply LiveClient::request(const nlohmann::json&, int) {
    throw std::runtime_error("this build has no ZMQ: it cannot reach a mime.live/1 session");
}

void LiveClient::startFrames(std::function<void(LiveFrame)>) {}

void LiveClient::stopFrames() {}

void LiveClient::frameLoop(std::function<void(LiveFrame)>) {}

#endif

void LiveClient::close() {
    closed_.store(true);
    stopFrames();
}

LiveClient::Stats LiveClient::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

} // namespace microbotica::live
