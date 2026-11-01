#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <exception>
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

/// What became of one request: its reply, or the error that took its place (no reply in time, a reply that is not
/// mime.live/1 or answers another command, the session closed).
struct LiveOutcome {
    nlohmann::json request;
    std::optional<Reply> reply;
    std::exception_ptr error;

    bool ok() const { return reply.has_value() && reply->ok; }
    /// The error said, or empty when a reply came.
    std::string errorText() const;
};

/// The transport of a mime.live/1 session (MIME's `mime serve`): ZMQ, REQ sockets for its commands and a SUB socket
/// for its frames. The messages themselves are live_protocol's; this class only moves them.
///
/// Nothing here blocks its caller unless asked to: requests go out on worker threads ("lanes"), each with its own REQ
/// socket, and their outcomes come back to a callback on that thread.
/// - The commands lane takes every command in the order posted, one at a time, as a REQ socket must: what a user
///   asks of the session reaches it in the order asked.
/// - The status lane takes `status` alone, so a poll is never queued behind a `set` waiting for the end of the chunk in
///   hand (MIME answers status at once, even while a chunk compiles).
///
/// Each request waits as long as MIME may take to answer it (LiveTimeouts); a request that times out recreates its
/// lane's socket, since a REQ socket whose reply never came cannot send again. A reply to another command than the one
/// sent is an error. close() ends every request in flight within 100 ms. The frames' socket connects before `hello` is
/// asked, so no frame sent after hello is missed; frames are received and decoded on a thread of their own and handed
/// to a sink there.
///
/// A callback runs on a lane's thread: it must not wait for another request of the same lane, and must not drop the
/// last reference to the client (whose destructor joins the lanes).
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
    using Outcome = LiveOutcome;
    using Done = std::function<void(const Outcome&)>;

    /// @param commands  the session's REP endpoint, e.g. tcp://127.0.0.1:5555
    /// @param frames    its PUB endpoint, e.g. tcp://127.0.0.1:5556
    LiveClient(std::string commands, std::string frames, Timeouts timeouts = {});
    ~LiveClient();
    LiveClient(const LiveClient&) = delete;
    LiveClient& operator=(const LiveClient&) = delete;

    /// Connect the frames socket, then ask `hello`, on the commands lane; `done` gets hello's reply (hello() is set
    /// before it is called) or the error: ProtocolError for a refused or unusable hello, std::runtime_error for no reply.
    void connectAsync(Done done);
    /// connectAsync, waited for. Throws its error.
    const Hello& connect();

    bool connected() const;
    const Hello& hello() const;

    /// Post one request to the commands lane, after those posted before it; `done` (may be empty) gets its outcome.
    /// It waits at most `timeoutMs` for its reply (the command's Timeouts when negative).
    void post(const nlohmann::json& message, Done done, int timeoutMs = -1);
    /// Post a `status` request to the status lane.
    void postStatus(Done done);

    /// post, waited for: the reply, or its error thrown (std::runtime_error when no reply comes, ProtocolError when
    /// the reply is not mime.live/1 or answers another command). Never from a lane's callback.
    Reply request(const nlohmann::json& message, int timeoutMs = -1);

    /// Receive frames on a thread: each decoded frame goes to `sink`, on that thread. Call after connect().
    void startFrames(std::function<void(LiveFrame)> sink);

    /// Stop receiving frames and join the thread. Safe to call more than once, from any thread but the frames'.
    void stopFrames();

    Stats stats() const;

    /// Wait, at most `timeoutMs`, for the session's end: its last frame (marked final) or its frames' connection
    /// closing. What follows a pending stop. True when either came.
    bool waitForEnd(int timeoutMs);

    /// The session is gone (its process ended), or no longer wanted: stop receiving, end the requests in flight and
    /// refuse every further one at once rather than wait for a reply that cannot come.
    void close();
    bool closed() const { return closed_.load(); }

    const Timeouts& timeouts() const { return timeouts_; }
    const std::string& commandsEndpoint() const { return commands_; }
    const std::string& framesEndpoint() const { return frames_; }

private:
    struct Sockets;
    class Lane;
    struct Job;
    friend class Lane;

    void run(Job& job, Outcome& outcome, void* laneSocket);
    void connectFrames();
    void frameLoop(std::function<void(LiveFrame)> sink);

    std::string commands_;
    std::string frames_;
    Timeouts timeouts_;
    std::unique_ptr<Sockets> sockets_;
    mutable std::mutex helloMutex_;
    std::optional<Hello> hello_;

    std::thread frameThread_;
    std::atomic<bool> receiving_{false};
    std::atomic<bool> closed_{false};
    mutable std::mutex statsMutex_;
    std::condition_variable ended_;  ///< the final frame, the connection closed, or close()
    Stats stats_;

    std::unique_ptr<Lane> commandsLane_;
    std::unique_ptr<Lane> statusLane_;
};

} // namespace microbotica::live
