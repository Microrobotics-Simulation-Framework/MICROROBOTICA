#include "experiment/experiment_runner.h"
#include <spdlog/spdlog.h>
#include <QProcessEnvironment>

#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace microbotica::experiment {

ExperimentRunner::ExperimentRunner(QObject* parent)
    : QObject(parent)
{
    MBCA_EXPERIMENTAL_WARN("ExperimentRunner");
}

ExperimentRunner::~ExperimentRunner() {
    stop();
}

QStringList ExperimentRunner::serveArguments(const ServeCommand& command) {
    QStringList args;
    args << QStringLiteral("serve") << command.folder
         << QStringLiteral("--commands") << QStringLiteral("tcp://127.0.0.1:0")
         << QStringLiteral("--frames") << QStringLiteral("tcp://127.0.0.1:0");
    if (!command.readyFile.isEmpty()) {
        args << QStringLiteral("--ready-file") << command.readyFile;
    }
    if (!command.out.isEmpty()) {
        args << QStringLiteral("--out") << command.out;
    }
    if (command.jsonFrames) {
        args << QStringLiteral("--json-frames");
    }
    return args;
}

bool ExperimentRunner::startServe(const ServeCommand& command) {
    if (process_ && process_->state() != QProcess::NotRunning) {
        spdlog::warn("ExperimentRunner: Process already running — stop it first");
        return false;
    }
    if (process_) {
        process_->deleteLater();
        process_ = nullptr;
    }

    intentional_stop_ = false;
    status_ = RunnerStatus::Starting;
    process_ = new QProcess(this);

    // JAX on the CPU unless the user's environment says otherwise: MIME 0.3
    // runs there by default, and a GPU must be chosen, never stumbled on.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains(QStringLiteral("JAX_PLATFORMS"))) {
        env.insert(QStringLiteral("JAX_PLATFORMS"), QStringLiteral("cpu"));
    }
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));  // the console sees lines as they come
    process_->setProcessEnvironment(env);

    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ExperimentRunner::onProcessFinished);
    connect(process_, &QProcess::started,
            this, &ExperimentRunner::onProcessStarted);
    // Forward stdout/stderr to anyone listening (MimeConsolePanel).
    connect(process_, &QProcess::readyReadStandardOutput, this, [this]() {
        Q_EMIT stdoutChunk(process_->readAllStandardOutput());
    });
    connect(process_, &QProcess::readyReadStandardError, this, [this]() {
        Q_EMIT stderrChunk(process_->readAllStandardError());
    });

    const QStringList args = serveArguments(command);
    spdlog::info("ExperimentRunner: {} {}", command.program.toStdString(),
                 args.join(QLatin1Char(' ')).toStdString());
    process_->start(command.program, args);
    if (!process_->waitForStarted(10000)) {
        spdlog::error("ExperimentRunner: could not start {}: {}", command.program.toStdString(),
                      process_->errorString().toStdString());
        status_ = RunnerStatus::Crashed;
        delete process_;
        process_ = nullptr;
        return false;
    }
    return true;
}

void ExperimentRunner::stop() {
    if (!process_ || process_->state() == QProcess::NotRunning) {
        return;
    }

    intentional_stop_ = true;
    spdlog::info("ExperimentRunner: Stopping the MIME process");
    // After a `stop` command, mime serve writes its run folder and exits on
    // its own; SIGTERM makes it do the same; only then, kill.
    if (!process_->waitForFinished(5000)) {
        process_->terminate();
        if (!process_->waitForFinished(15000)) {
            spdlog::warn("ExperimentRunner: the MIME process did not stop — killing");
            process_->kill();
            process_->waitForFinished(3000);
        }
    }
    status_ = RunnerStatus::Stopped;
}

bool ExperimentRunner::isRunning() const {
    return process_ && process_->state() == QProcess::Running;
}

namespace {

/// Single non-blocking TCP probe of 127.0.0.1:port. Used by both the
/// blocking waitUntilReady (in a sleep loop) and the async probeReady
/// (driven from a QTimer).
bool probeLocalhostPort(int port, int select_timeout_ms) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags | O_NONBLOCK);

    int rc = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    bool connected = (rc == 0);
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(s, &wfds);
        timeval tv{select_timeout_ms / 1000,
                    (select_timeout_ms % 1000) * 1000};
        int sel = ::select(s + 1, nullptr, &wfds, nullptr, &tv);
        if (sel > 0) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            if (::getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen) == 0
                && err == 0) {
                connected = true;
            }
        }
    }
    ::close(s);
    return connected;
}

} // namespace

bool ExperimentRunner::probeReady(int port, int timeout_ms) {
    if (!process_ || process_->state() != QProcess::Running) {
        return false;
    }
    return probeLocalhostPort(port, timeout_ms);
}

bool ExperimentRunner::probePort(int port, int timeout_ms) {
    return probeLocalhostPort(port, timeout_ms);
}

bool ExperimentRunner::waitUntilReady(int timeout_ms, int port) {
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now()
        + std::chrono::milliseconds(timeout_ms);

    while (clock::now() < deadline) {
        // Bail if the subprocess died during startup.
        if (!process_ || process_->state() != QProcess::Running) {
            spdlog::warn("ExperimentRunner::waitUntilReady: subprocess "
                          "is no longer running");
            return false;
        }

        if (probeLocalhostPort(port, 200)) {
            spdlog::info("ExperimentRunner::waitUntilReady: ZMQ port {} "
                          "is accepting", port);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    spdlog::error("ExperimentRunner::waitUntilReady: port {} did not "
                   "accept within {} ms", port, timeout_ms);
    return false;
}

void ExperimentRunner::onProcessStarted() {
    status_ = RunnerStatus::Running;
    spdlog::info("ExperimentRunner: MIME runner started (PID {})",
                 process_->processId());
    Q_EMIT processStarted();
}

void ExperimentRunner::onProcessFinished(int exit_code, QProcess::ExitStatus exit_status) {
    if (intentional_stop_) {
        status_ = RunnerStatus::Stopped;
        spdlog::info("ExperimentRunner: MIME runner stopped (exit code {})", exit_code);
        Q_EMIT processStopped();
        return;
    }

    status_ = RunnerStatus::Crashed;
    QString stderr_output;
    if (process_) {
        stderr_output = process_->readAllStandardError();
    }

    spdlog::warn("ExperimentRunner: MIME runner crashed (exit code {}, status {}): {}",
                 exit_code,
                 exit_status == QProcess::CrashExit ? "crash" : "normal",
                 stderr_output.toStdString());

    Q_EMIT processCrashed(exit_code, stderr_output);
    Q_EMIT processStopped();
}

} // namespace microbotica::experiment
