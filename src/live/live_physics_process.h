#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

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
/// Nothing but stop() waits for the session: start, pause, resume and set are posted to the client's commands lane, in
/// order, and their outcomes go to the command observer as they come. requestStop() stops without waiting.
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
            .component_version = "1.2.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "A MIME 0.3 live session (mime.live/1, mime serve) as a PhysicsProcess.",
            .preconditions     = {"The client is connected: hello was answered"},
            .postconditions    = {"Every frame received is mapped through the stage binding to a ResultFrame",
                                  "stop() sends stop, which stops the session in every state",
                                  "A stop taken while a chunk runs completes once the session's final frame "
                                  "arrives or its connection closes",
                                  "launch, sendParameters and setPaused return without waiting for a reply"},
            .invariants        = {"Positions are converted from metres to the stage's units",
                                  "Commands reach the session in the order they were given"},
            .design_rationale  = "Keeps the scene pipeline (SimulationController, ResultsApplicator) as it was; "
                                 "the protocol's shapes stay in live_protocol, its transport in LiveClient, whose "
                                 "lanes keep the caller (the UI thread) from waiting on MIME.",
            .assumptions       = {"The session's frames are the binding's fields (mime serve's default selection)"},
            .limitations       = {"A refused command is reported to the observer, not raised",
                                  "A stop taken while a chunk runs names no run folder: MIME writes it after the "
                                  "final frame, where `mime serve --out` said"},
            .validated_regimes = {},
            .hazard_hints      = {
                "A dropped frame is counted and said; the scene shows the next frame received.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    /// Starts receiving frames and posts `start` (the session steps, paced). With startOnLaunch false, launch only
    /// starts receiving.
    void launch(const core::PhysicsConfig& config) override;
    std::optional<core::ResultFrame> receiveResult() override;
    /// Posts `set` for the controller's live inputs: `params` is {label: value}.
    void sendParameters(const nlohmann::json& params) override;
    /// Posts `pause`, or `start` to resume.
    void setPaused(bool paused) override;
    /// requestStop, waited for.
    void stop() override;
    core::ProcessStatus status() const override;
    core::StreamStats streamStats() const override;
    /// The session pauses itself (no frames while paused), and a step while paused sends frames that must be shown.
    bool pausesItself() const override { return true; }

    /// Post `stop` (after the commands posted before it) and return: `done` runs, on a client thread, once the session
    /// has stopped (stop's reply, or for a stop taken while a chunk ran, its final frame or closed connection; or the
    /// stop failed), when the stream has ended and status() is Stopped. A second call adds its `done` to the first's.
    void requestStop(std::function<void()> done);

    /// Every command's outcome, accepted, refused or failed, as it comes (on a client thread): the console shows
    /// refusals, the run panel the status.
    void setCommandObserver(std::function<void(const LiveOutcome&)> observer);
    /// Every frame as it arrives (on the frames thread): the plots read the scalars.
    void setFrameObserver(std::function<void(const LiveFrame&)> observer);
    void setStartOnLaunch(bool start) { startOnLaunch_ = start; }

    /// stop's run folder, once the session has stopped, when stop's reply named it (a stop between chunks).
    std::optional<std::string> runFolder() const;
    /// The stop was taken while a chunk ran (pending): its reply named no run folder.
    bool stoppedPending() const;

private:
    /// What the client's callbacks touch, shared so that a callback that comes late never reaches a process gone.
    struct State {
        mutable std::mutex mutex;
        std::function<void(const LiveOutcome&)> commandObserver;
        std::function<void(const LiveFrame&)> frameObserver;
        std::optional<std::string> runFolder;
        bool stoppedPending = false;
        bool stopRequested = false;
        bool stopped = false;
        std::vector<std::function<void()>> onStopped;
        std::atomic<core::ProcessStatus> status{core::ProcessStatus::Idle};
        core::ThreadSafeQueue<std::optional<core::ResultFrame>> queue;
    };

    /// Post a command; its outcome goes to the observer, then to `then`.
    void send(const nlohmann::json& message, std::function<void(const LiveOutcome&)> then = {});

    std::shared_ptr<LiveClient> client_;
    std::shared_ptr<const BindingFrames> frames_;
    std::shared_ptr<State> state_;
    bool startOnLaunch_ = true;
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
