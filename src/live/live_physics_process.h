#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "core/compute_backend.h"
#include "core/physics_process.h"
#include "core/stability.h"
#include "core/threadsafe_queue.h"
#include "live/binding_frames.h"
#include "live/live_client.h"
#include "stubs/stub_render_session.h"

namespace microbotica::live {

/// A mime.live/1 session as MICROROBOTICA's PhysicsProcess: its frames, through the stage binding, become the
/// ResultFrames SimulationController applies to the scene; launch, pause and stop are the session's start, pause and
/// stop; sendParameters sets the controller's live inputs.
///
/// MBCA-IMPL-013
class LivePhysicsProcess : public core::PhysicsProcess {
public:
    /// @param client  a connected client (hello asked)
    /// @param frames  the binding with the stage's units
    LivePhysicsProcess(std::shared_ptr<LiveClient> client, std::shared_ptr<const BindingFrames> frames);
    ~LivePhysicsProcess() override;

    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-IMPL-013",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "A MIME 0.3 live session (mime.live/1, mime serve) as a PhysicsProcess.",
            .preconditions     = {"The client is connected: hello was answered"},
            .postconditions    = {"Every frame received is mapped through the stage binding to a ResultFrame",
                                  "stop() sends stop, which stops the session in every state"},
            .invariants        = {"Positions are converted from metres to the stage's units"},
            .design_rationale  = "Keeps the scene pipeline (SimulationController, ResultsApplicator) as it was; "
                                 "the protocol's shapes stay in live_protocol, its transport in LiveClient.",
            .assumptions       = {"The session's frames are the binding's fields (mime serve's default selection)"},
            .limitations       = {"A refused command is reported to the observer, not raised"},
            .validated_regimes = {},
            .hazard_hints      = {
                "A dropped frame is counted and said; the scene shows the next frame received.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    /// Sends `start` (the session steps, paced). With startOnLaunch false, launch only starts receiving.
    void launch(const core::PhysicsConfig& config) override;
    std::optional<core::ResultFrame> receiveResult() override;
    /// Sets the controller's live inputs: `params` is {label: value}.
    void sendParameters(const nlohmann::json& params) override;
    void setPaused(bool paused) override;
    void stop() override;
    core::ProcessStatus status() const override;
    core::StreamStats streamStats() const override;
    /// The session pauses itself (no frames while paused), and a step while paused sends frames that must be shown.
    bool pausesItself() const override { return true; }

    /// Every reply, accepted or refused, as it arrives (on the calling thread): the console shows refusals.
    void setReplyObserver(std::function<void(const Reply&)> observer);
    /// Every frame as it arrives (on the frames thread): the plots read the scalars.
    void setFrameObserver(std::function<void(const LiveFrame&)> observer);
    void setStartOnLaunch(bool start) { startOnLaunch_ = start; }

    /// stop's run folder, once the session has stopped.
    std::optional<std::string> runFolder() const;

private:
    std::optional<Reply> send(const nlohmann::json& message, int timeoutMs = -1);

    std::shared_ptr<LiveClient> client_;
    std::shared_ptr<const BindingFrames> frames_;
    core::ThreadSafeQueue<std::optional<core::ResultFrame>> queue_;
    std::atomic<core::ProcessStatus> status_{core::ProcessStatus::Idle};
    std::atomic<bool> finished_{false};
    bool startOnLaunch_ = true;
    mutable std::mutex mutex_;
    std::function<void(const Reply&)> replyObserver_;
    std::function<void(const LiveFrame&)> frameObserver_;
    std::optional<std::string> runFolder_;
};

/// The compute backend of a live session: its PhysicsProcess shares the session's client.
class LiveComputeBackend : public core::ComputeBackend {
public:
    LiveComputeBackend(std::shared_ptr<LiveClient> client, std::shared_ptr<const BindingFrames> frames,
                       std::function<void(LivePhysicsProcess&)> configure = {})
        : client_(std::move(client)), frames_(std::move(frames)), configure_(std::move(configure)) {}

    std::unique_ptr<core::PhysicsProcess> createPhysicsProcess() override {
        auto process = std::make_unique<LivePhysicsProcess>(client_, frames_);
        if (configure_) {
            configure_(*process);
        }
        return process;
    }

    std::unique_ptr<core::RenderSession> createSession(const core::RenderConfig& /*config*/) override {
        return std::make_unique<stubs::StubRenderSession>();
    }

    std::string backendName() const override { return "mime.live/1"; }

private:
    std::shared_ptr<LiveClient> client_;
    std::shared_ptr<const BindingFrames> frames_;
    std::function<void(LivePhysicsProcess&)> configure_;
};

} // namespace microbotica::live
