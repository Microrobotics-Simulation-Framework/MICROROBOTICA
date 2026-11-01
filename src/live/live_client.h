#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "live/live_protocol.h"

namespace microbotica::live {

/// The transport of a mime.live/1 session (MIME's `mime serve`): ZMQ, a REQ socket for its commands and a SUB socket
/// for its frames. The messages themselves are live_protocol's; this class only moves them.
///
/// The frames' socket connects before `hello` is asked, so no frame sent after hello is missed. Requests are
/// serialised (one REQ socket, guarded); a request that times out recreates the socket, since a REQ socket whose reply
/// never came cannot send again. Frames are received and decoded on a thread of their own and handed to a sink there.
class LiveClient {
public:
    struct Stats {
        std::uint64_t framesReceived = 0;
        std::uint64_t bytesReceived = 0;
        std::uint64_t decodeErrors = 0;
        std::uint64_t dropped = 0;      ///< frames a gap in the numbers says were lost
        std::uint64_t outOfOrder = 0;
        std::uint32_t lastFrameBytes = 0;
        double decodeAvgUs = 0.0;
        double decodeMaxUs = 0.0;
        bool finalSeen = false;         ///< the session's last frame arrived
        std::string lastProblem;        ///< the last decode or order problem, said
    };

    /// @param commands  the session's REP endpoint, e.g. tcp://127.0.0.1:5555
    /// @param frames    its PUB endpoint, e.g. tcp://127.0.0.1:5556
    /// @param timeoutMs how long a request waits for its reply
    LiveClient(std::string commands, std::string frames, int timeoutMs = 30000);
    ~LiveClient();
    LiveClient(const LiveClient&) = delete;
    LiveClient& operator=(const LiveClient&) = delete;

    /// Connect the frames socket, then ask `hello`. Throws ProtocolError (a refused or unusable hello) or
    /// std::runtime_error (no reply: the transport).
    const Hello& connect();

    bool connected() const { return hello_.has_value(); }
    const Hello& hello() const;

    /// Send one request and return its reply, waiting at most `timeoutMs` (the constructor's when negative).
    /// Throws std::runtime_error when no reply comes, ProtocolError when the reply is not mime.live/1.
    Reply request(const nlohmann::json& message, int timeoutMs = -1);

    /// Receive frames on a thread: each decoded frame goes to `sink`, on that thread. Call after connect().
    void startFrames(std::function<void(LiveFrame)> sink);

    /// Stop receiving frames and join the thread. Safe to call more than once.
    void stopFrames();

    Stats stats() const;

    /// The session is gone (its process ended): stop receiving, and refuse every further request at once rather than
    /// wait for a reply that cannot come.
    void close();
    bool closed() const { return closed_.load(); }

    const std::string& commandsEndpoint() const { return commands_; }
    const std::string& framesEndpoint() const { return frames_; }

private:
    struct Sockets;
    void frameLoop(std::function<void(LiveFrame)> sink);

    std::string commands_;
    std::string frames_;
    int timeoutMs_;
    std::unique_ptr<Sockets> sockets_;
    std::mutex requestMutex_;
    std::optional<Hello> hello_;

    std::thread frameThread_;
    std::atomic<bool> receiving_{false};
    std::atomic<bool> closed_{false};
    mutable std::mutex statsMutex_;
    Stats stats_;
};

} // namespace microbotica::live
