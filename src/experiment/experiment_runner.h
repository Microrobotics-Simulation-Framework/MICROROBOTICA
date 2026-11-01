#pragma once

#include <string>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include "core/component_meta.h"
#include "core/stability.h"

namespace microbotica::experiment {

/// Manages the lifecycle of a MIME physics subprocess.
///
/// MBCA-COMP-061
///
/// Spawns MIME 0.3's `mime serve <saved folder>` via QProcess, monitors
/// the process for crashes, and provides status reporting. `mime serve`
/// loads and builds the saved experiment and serves its live session
/// (mime.live/1) over ZMQ. This class manages the process, not the
/// session (that's live::LiveClient).
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
            .component_version = "2.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Manages the MIME process (mime serve) lifecycle via QProcess.",
            .preconditions     = {"MIME 0.3 is installed: the `mime` command (or the path Settings names) runs"},
            .postconditions    = {"startServe() spawns `mime serve <saved folder>` on localhost at free ports",
                                  "stop() lets it write its run folder, then SIGTERMs, then kills",
                                  "processCrashed signal emitted on unexpected exit"},
            .invariants        = {"At most one MIME process is active at a time"},
            .design_rationale  = "Subprocess isolation keeps JAX in a separate process "
                                 "from the Qt/OpenGL UI, preventing context conflicts.",
            .assumptions       = {"mime serve writes its ready file once its sockets listen",
                                  "mime serve stops cleanly, writing its run folder, on stop or SIGTERM"},
            .limitations       = {"No automatic restart on crash (caller must re-start)"},
            .validated_regimes = {},
            .hazard_hints      = {
                "A crashed MIME process sends no final frame; the live client's frame "
                "stream then simply ends.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// What `mime serve` is run with (MIME 0.3's live session, mime.live/1).
    struct ServeCommand {
        QString program = QStringLiteral("mime");  ///< the `mime` command, or its full path
        QString folder;      ///< the saved experiment it serves
        QString readyFile;   ///< --ready-file: written once its sockets listen
        QString out;         ///< --out: the run folder it writes when it stops
        bool jsonFrames = false;  ///< --json-frames, for debugging
    };

    /// The arguments `mime serve` is given: the folder, both sockets on
    /// localhost at free ports, the ready file and the run folder.
    static QStringList serveArguments(const ServeCommand& command);

    /// Spawn `mime serve` for a saved experiment. Its stdout and stderr are
    /// forwarded (stdoutChunk, stderrChunk). JAX stays on the CPU unless
    /// JAX_PLATFORMS is already set. Returns false if it could not start.
    bool startServe(const ServeCommand& command);

    /// Block until the runner's ZMQ REP port (default 5555) accepts a
    /// connection, or timeout. This is a *liveness probe* — useful
    /// after start() because waitForStarted only confirms exec, not
    /// that the Python interpreter has finished JIT-compiling the
    /// graph and bound its sockets.
    /// @return true if the port is accepting within timeout_ms.
    bool waitUntilReady(int timeout_ms = 30000, int port = 5555);

    /// Single non-blocking TCP probe of localhost:port with a short
    /// select() timeout. Returns true if the port accepts a connection
    /// within `timeout_ms` (default 100). Intended to be driven from a
    /// QTimer so the UI thread stays responsive while the MIME runner
    /// is JIT-compiling.
    bool probeReady(int port = 5555, int timeout_ms = 100);

    /// Process-less version of probeReady — does the same TCP accept
    /// probe but does not require ExperimentRunner to own a child
    /// process. Useful for non-Local paths (Cloud SSH tunnel) where the
    /// port is bound by something we don't manage.
    static bool probePort(int port, int timeout_ms = 100);

    /// Stop the MIME process: wait for it to exit on its own (after a `stop`
    /// command it writes its run folder and exits), then SIGTERM (which
    /// `mime serve` also answers by stopping cleanly), then kill. Blocks.
    void stop();

    /// The process is about to exit because it was told to (a session's
    /// `stop`): its exit is a stop, not a crash.
    void expectExit();

    /// Stop the MIME process without blocking: let it exit on its own for
    /// `graceMs` (writing its run folder), then SIGTERM, then after 15 s more
    /// kill. processStopped says when it has exited.
    void stopAsync(int graceMs);

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
