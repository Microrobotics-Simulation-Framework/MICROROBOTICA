#pragma once

#include <atomic>
#include <condition_variable>
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

/// How long a request waits for its reply, by when MIME answers it (live_protocol's ReplyWait).
struct LiveTimeouts {
    int atOnceMs = 5000;       ///< hello, status, pause (and a stop taken while a chunk runs)
    int chunkEndMs = 30000;    ///< set, record, pace, start, step: allows a first chunk's compile (A0's is 3 to 4 s)
    int runFolderMs = 120000;  ///< stop between chunks, which replies once its run folder is written

    static LiveTimeouts all(int ms) { return LiveTimeouts{ms, ms, ms}; }
    int forCommand(const std::string& command) const;
};

/// The transport of a mime.live/1 session (MIME's `mime serve`): ZMQ, a REQ socket for its commands and a SUB socket
/// for its frames. The messages themselves are live_protocol's; this class only moves them.
///
/// The frames' socket connects before `hello` is asked, so no frame sent after hello is missed. Requests are
/// serialised (one REQ socket, guarded); a request that times out recreates the socket, since a REQ socket whose reply
/// never came cannot send again. Each waits as long as MIME may take to answer it (live_protocol's ReplyWait): the
/// server answers hello, status, pause and a pending stop at once, even while a chunk compiles, and the commands that
/// change the run at the end of the chunk in hand. A reply to another command than the one sent is refused. Frames are
/// received and decoded on a thread of their own and handed to a sink there.
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
        bool disconnected = false;      ///< the frames' connection closed (the server is gone)
        std::string lastProblem;        ///< the last decode or order problem, said
    };

    using Timeouts = LiveTimeouts;

    /// @param commands  the session's REP endpoint, e.g. tcp://127.0.0.1:5555
    /// @param frames    its PUB endpoint, e.g. tcp://127.0.0.1:5556
    LiveClient(std::string commands, std::string frames, Timeouts timeouts = {});
    ~LiveClient();
    LiveClient(const LiveClient&) = delete;
    LiveClient& operator=(const LiveClient&) = delete;

    /// Connect the frames socket, then ask `hello`. Throws ProtocolError (a refused or unusable hello) or
    /// std::runtime_error (no reply: the transport).
    const Hello& connect();

    bool connected() const { return hello_.has_value(); }
    const Hello& hello() const;

    /// Send one request and return its reply, waiting at most `timeoutMs` (the command's Timeouts when negative).
    /// Throws std::runtime_error when no reply comes, ProtocolError when the reply is not mime.live/1 or answers
    /// another command.
    Reply request(const nlohmann::json& message, int timeoutMs = -1);

    /// Receive frames on a thread: each decoded frame goes to `sink`, on that thread. Call after connect().
    void startFrames(std::function<void(LiveFrame)> sink);

    /// Stop receiving frames and join the thread. Safe to call more than once.
    void stopFrames();

    Stats stats() const;

    /// Wait, at most `timeoutMs`, for the session's end: its last frame (marked final) or its frames' connection
    /// closing. What follows a pending stop. True when either came.
    bool waitForEnd(int timeoutMs);

    /// The session is gone (its process ended): stop receiving, and refuse every further request at once rather than
    /// wait for a reply that cannot come.
    void close();
    bool closed() const { return closed_.load(); }

    const Timeouts& timeouts() const { return timeouts_; }
    const std::string& commandsEndpoint() const { return commands_; }
    const std::string& framesEndpoint() const { return frames_; }

private:
    struct Sockets;
    void frameLoop(std::function<void(LiveFrame)> sink);

    std::string commands_;
    std::string frames_;
    Timeouts timeouts_;
    std::unique_ptr<Sockets> sockets_;
    std::mutex requestMutex_;
    std::optional<Hello> hello_;

    std::thread frameThread_;
    std::atomic<bool> receiving_{false};
    std::atomic<bool> closed_{false};
    mutable std::mutex statsMutex_;
    std::condition_variable ended_;  ///< the final frame, the connection closed, or close()
    Stats stats_;
};

} // namespace microbotica::live
