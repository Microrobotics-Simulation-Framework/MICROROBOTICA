#pragma once

#include <string>
#include <QObject>
#include <QProcess>
#include "core/component_meta.h"
#include "core/stability.h"
#include "experiment/experiment_config.h"

namespace microbotica::experiment {

/// Manages the lifecycle of a MIME physics subprocess.
///
/// MBCA-COMP-061
///
/// Spawns `python3 -m mime.runner <experiment_dir>` via QProcess,
/// monitors the process for crashes, and provides status reporting.
/// The MIME runner reads experiment.yaml, builds the graph, starts
/// stepping, and publishes ResultFrames over ZMQ. This class manages
/// the process, not the ZMQ connection (that's MimePhysicsProcess).
class ExperimentRunner : public QObject {
    Q_OBJECT

public:
    enum class RunnerStatus {
        Idle,       ///< No process running
        Starting,   ///< Process launched, waiting for ZMQ to become available
        Running,    ///< Process running and publishing frames
        Crashed,    ///< Process exited unexpectedly
        Stopped     ///< Process stopped by user
    };

    explicit ExperimentRunner(QObject* parent = nullptr);
    ~ExperimentRunner() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-061",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Manages MIME physics subprocess lifecycle via QProcess.",
            .preconditions     = {"python3 is available on PATH",
                                  "mime package is installed in the Python environment"},
            .postconditions    = {"start() spawns a MIME runner subprocess",
                                  "stop() terminates the subprocess gracefully",
                                  "processCrashed signal emitted on unexpected exit"},
            .invariants        = {"At most one MIME process is active at a time"},
            .design_rationale  = "Subprocess isolation keeps JAX/CUDA in a separate process "
                                 "from the Qt/OpenGL UI, preventing context conflicts.",
            .assumptions       = {"MIME runner publishes on ZMQ ports 5555/5556 by default",
                                  "MIME runner exits cleanly on SIGTERM"},
            .limitations       = {"No automatic restart on crash (caller must re-start)",
                                  "No stdout/stderr capture beyond crash diagnostics"},
            .validated_regimes = {},
            .hazard_hints      = {
                "A crashed MIME process leaves ZMQ sockets in a half-closed state. "
                "MimePhysicsProcess detects this via the 10s heartbeat timeout.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Start a MIME runner subprocess for the given experiment directory.
    /// @param experiment_dir  Absolute path to the experiment directory
    /// @return true if the process started successfully
    bool start(const std::string& experiment_dir);

    /// Block until the runner's ZMQ REP port (default 5555) accepts a
    /// connection, or timeout. This is a *liveness probe* — useful
    /// after start() because waitForStarted only confirms exec, not
    /// that the Python interpreter has finished JIT-compiling the
    /// graph and bound its sockets.
    /// @return true if the port is accepting within timeout_ms.
    bool waitUntilReady(int timeout_ms = 30000, int port = 5555);

    /// Stop the running MIME subprocess gracefully.
    void stop();

    /// @return Current runner status
    RunnerStatus runnerStatus() const { return status_; }

    /// @return true if a MIME process is currently running
    bool isRunning() const;

Q_SIGNALS:
    /// Emitted when the MIME process exits unexpectedly.
    void processCrashed(int exit_code, const QString& stderr_output);

    /// Emitted when the MIME process starts successfully.
    void processStarted();

    /// Emitted when the MIME process stops (intentionally or not).
    void processStopped();

    /// Emitted as the MIME subprocess produces stdout/stderr lines.
    /// Wire to MimeConsolePanel::appendStdout / appendStderr.
    void stdoutChunk(const QByteArray& data);
    void stderrChunk(const QByteArray& data);

private Q_SLOTS:
    void onProcessFinished(int exit_code, QProcess::ExitStatus exit_status);
    void onProcessStarted();

private:
    QProcess* process_ = nullptr;
    RunnerStatus status_ = RunnerStatus::Idle;
    bool intentional_stop_ = false;
};

} // namespace microbotica::experiment
