#include "live/live_physics_process.h"

#include <future>

#include <spdlog/spdlog.h>

namespace microbotica::live {

LivePhysicsProcess::LivePhysicsProcess(std::shared_ptr<LiveClient> client, std::shared_ptr<const BindingFrames> frames)
    : client_(std::move(client)), frames_(std::move(frames)), state_(std::make_shared<State>())
{
    MBCA_EXPERIMENTAL_WARN("LivePhysicsProcess");
}

LivePhysicsProcess::~LivePhysicsProcess() {
    client_->stopFrames();
}

void LivePhysicsProcess::setCommandObserver(std::function<void(const LiveOutcome&)> observer) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->commandObserver = std::move(observer);
}

void LivePhysicsProcess::setFrameObserver(std::function<void(const LiveFrame&)> observer) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->frameObserver = std::move(observer);
}

std::optional<std::string> LivePhysicsProcess::runFolder() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->runFolder;
}

bool LivePhysicsProcess::stoppedPending() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->stoppedPending;
}

void LivePhysicsProcess::send(const nlohmann::json& message, std::function<void(const LiveOutcome&)> then) {
    // The callback holds the state, never the client: the client's destructor joins the thread it runs on.
    client_->post(message, [state = state_, then = std::move(then)](const LiveOutcome& o) {
        if (o.error) {
            spdlog::error("LivePhysicsProcess: {}", o.errorText());
        } else if (!o.reply->ok) {
            spdlog::warn("LivePhysicsProcess: {} refused: {}", o.reply->command,
                         o.reply->errors.empty() ? std::string("(no reason given)")
                                                 : o.reply->errors.front().code + ": " +
                                                       o.reply->errors.front().message);
        }
        std::function<void(const LiveOutcome&)> observer;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            observer = state->commandObserver;
        }
        if (observer) {
            observer(o);
        }
        if (then) {
            then(o);
        }
    });
}

void LivePhysicsProcess::launch(const core::PhysicsConfig& /*config*/) {
    state_->status.store(core::ProcessStatus::Launching);
    client_->startFrames([state = state_, frames = frames_](LiveFrame frame) {
        std::function<void(const LiveFrame&)> observer;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            observer = state->frameObserver;
        }
        if (observer) {
            observer(frame);
        }
        const bool last = frame.final;
        state->queue.push(frames->toResultFrame(frame));
        if (last) {
            state->queue.push(std::nullopt);  // the session's last frame: the stream ends here
        }
    });
    state_->status.store(core::ProcessStatus::Running);
    if (startOnLaunch_) {
        send(startRequest(), [state = state_](const LiveOutcome& o) {
            if (!o.ok()) {
                state->status.store(core::ProcessStatus::Error);
            }
        });
    }
}

std::optional<core::ResultFrame> LivePhysicsProcess::receiveResult() {
    return state_->queue.wait_pop();
}

void LivePhysicsProcess::sendParameters(const nlohmann::json& params) {
    if (params.is_object() && !params.empty()) {
        send(setRequest(params));
    }
}

void LivePhysicsProcess::setPaused(bool paused) {
    send(paused ? pauseRequest() : startRequest());
}

void LivePhysicsProcess::requestStop(std::function<void()> done) {
    {
        std::unique_lock<std::mutex> lock(state_->mutex);
        if (state_->stopped) {
            lock.unlock();
            if (done) done();  // already stopped: nothing to wait for
            return;
        }
        if (done) state_->onStopped.push_back(std::move(done));
        if (state_->stopRequested) {
            return;  // the first request's completion calls this one's done too
        }
        state_->stopRequested = true;
    }
    // Between chunks, stop writes the run folder before it replies (its timeout allows it). Taken while a chunk runs,
    // it is answered at once as pending: the session stops at the chunk's end, its final frame (or its connection
    // closing) says it has, and its run folder is written after that (MIME's client note, entry 23). The client
    // outlives every callback of its lanes, so the raw pointer is safe there.
    LiveClient* client = client_.get();
    send(stopRequest(), [state = state_, client](const LiveOutcome& o) {
        if (o.reply && o.reply->folder) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->runFolder = o.reply->folder;
        } else if (o.reply && o.reply->stopPending()) {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->stoppedPending = true;
            }
            if (!client->waitForEnd(client->timeouts().chunkEndMs)) {
                spdlog::warn("LivePhysicsProcess: a pending stop, and no final frame within {} ms",
                             client->timeouts().chunkEndMs);
            }
        }
        client->stopFrames();
        state->queue.push(std::nullopt);  // unblock receiveResult
        state->status.store(core::ProcessStatus::Stopped);
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->stopped = true;
            callbacks.swap(state->onStopped);
        }
        for (auto& callback : callbacks) {
            callback();
        }
    });
}

void LivePhysicsProcess::stop() {
    if (state_->status.load() == core::ProcessStatus::Stopped) {
        return;
    }
    std::promise<void> stopped;
    auto future = stopped.get_future();
    requestStop([&stopped] { stopped.set_value(); });
    future.wait();
}

core::ProcessStatus LivePhysicsProcess::status() const {
    return state_->status.load();
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
