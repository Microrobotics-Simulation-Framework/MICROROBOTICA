#include "live/live_physics_process.h"

#include <spdlog/spdlog.h>

namespace microbotica::live {

LivePhysicsProcess::LivePhysicsProcess(std::shared_ptr<LiveClient> client, std::shared_ptr<const BindingFrames> frames)
    : client_(std::move(client)), frames_(std::move(frames))
{
    MBCA_EXPERIMENTAL_WARN("LivePhysicsProcess");
}

LivePhysicsProcess::~LivePhysicsProcess() {
    client_->stopFrames();
}

void LivePhysicsProcess::setReplyObserver(std::function<void(const Reply&)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    replyObserver_ = std::move(observer);
}

void LivePhysicsProcess::setFrameObserver(std::function<void(const LiveFrame&)> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    frameObserver_ = std::move(observer);
}

std::optional<std::string> LivePhysicsProcess::runFolder() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runFolder_;
}

std::optional<Reply> LivePhysicsProcess::send(const nlohmann::json& message, int timeoutMs) {
    try {
        Reply reply = client_->request(message, timeoutMs);
        std::function<void(const Reply&)> observer;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observer = replyObserver_;
        }
        if (!reply.ok) {
            spdlog::warn("LivePhysicsProcess: {} refused: {}", reply.command,
                         reply.errors.empty() ? std::string("(no reason given)")
                                              : reply.errors.front().code + ": " + reply.errors.front().message);
        }
        if (observer) {
            observer(reply);
        }
        return reply;
    } catch (const std::exception& e) {
        spdlog::error("LivePhysicsProcess: {}", e.what());
        return std::nullopt;
    }
}

void LivePhysicsProcess::launch(const core::PhysicsConfig& /*config*/) {
    status_.store(core::ProcessStatus::Launching);
    client_->startFrames([this](LiveFrame frame) {
        std::function<void(const LiveFrame&)> observer;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observer = frameObserver_;
        }
        if (observer) {
            observer(frame);
        }
        const bool last = frame.final;
        queue_.push(frames_->toResultFrame(frame));
        if (last) {
            finished_.store(true);
            queue_.push(std::nullopt);  // the session's last frame: the stream ends here
        }
    });
    if (startOnLaunch_) {
        const auto reply = send(startRequest());
        if (!reply || !reply->ok) {
            status_.store(core::ProcessStatus::Error);
            return;
        }
    }
    status_.store(core::ProcessStatus::Running);
}

std::optional<core::ResultFrame> LivePhysicsProcess::receiveResult() {
    return queue_.wait_pop();
}

void LivePhysicsProcess::sendParameters(const nlohmann::json& params) {
    if (params.is_object() && !params.empty()) {
        send(setRequest(params));
    }
}

void LivePhysicsProcess::setPaused(bool paused) {
    send(paused ? pauseRequest() : startRequest());
}

void LivePhysicsProcess::stop() {
    if (status_.load() == core::ProcessStatus::Stopped) {
        return;
    }
    // stop writes the run folder before it replies: give it time.
    if (const auto reply = send(stopRequest(), 120000); reply && reply->folder) {
        std::lock_guard<std::mutex> lock(mutex_);
        runFolder_ = reply->folder;
    }
    client_->stopFrames();
    queue_.push(std::nullopt);  // unblock receiveResult
    status_.store(core::ProcessStatus::Stopped);
}

core::ProcessStatus LivePhysicsProcess::status() const {
    return status_.load();
}

core::StreamStats LivePhysicsProcess::streamStats() const {
    const LiveClient::Stats s = client_->stats();
    core::StreamStats out;
    out.format = "mime.live/1";
    out.compression = "none";
    out.framesReceived = s.framesReceived;
    out.framesSkipped = s.dropped;
    out.bytesReceived = s.bytesReceived;
    out.decodeErrors = s.decodeErrors;
    out.lastFrameBytes = s.lastFrameBytes;
    out.decodeAvgUs = s.decodeAvgUs;
    out.decodeMaxUs = s.decodeMaxUs;
    return out;
}

} // namespace microbotica::live
