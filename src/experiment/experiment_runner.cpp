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

bool ExperimentRunner::start(const std::string& experiment_dir,
                              const std::string& stream_format) {
    if (process_ && process_->state() != QProcess::NotRunning) {
        spdlog::warn("ExperimentRunner: Process already running — stop it first");
        return false;
    }

    intentional_stop_ = false;
    status_ = RunnerStatus::Starting;

    process_ = new QProcess(this);

    // Request a ResultFrame wire format from the runner (fit-up §8). The
    // subprocess otherwise inherits this process's environment unchanged.
    if (!stream_format.empty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("MIME_STREAM_FORMAT",
                   QString::fromStdString(stream_format));
        process_->setProcessEnvironment(env);
        spdlog::info("ExperimentRunner: requesting MIME_STREAM_FORMAT={}",
                     stream_format);
    }

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

    QStringList args;
    args << "-u"  // unbuffered so the panel sees output line-by-line
         << "-m" << "mime.runner" << QString::fromStdString(experiment_dir);

    spdlog::info("ExperimentRunner: Launching MIME runner for {}", experiment_dir);
    process_->start("python3", args);

    if (!process_->waitForStarted(10000)) {
        spdlog::error("ExperimentRunner: Failed to start MIME runner: {}",
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
    spdlog::info("ExperimentRunner: Stopping MIME runner");
    process_->terminate();
    if (!process_->waitForFinished(5000)) {
        spdlog::warn("ExperimentRunner: MIME runner did not terminate — killing");
        process_->kill();
        process_->waitForFinished(3000);
    }
    status_ = RunnerStatus::Stopped;
}

bool ExperimentRunner::isRunning() const {
    return process_ && process_->state() == QProcess::Running;
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

        // Try to open a TCP connection to localhost:<port>. If it
        // succeeds, ZMQ has bound and we're good. If it refuses, retry.
        int s = ::socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        // Non-blocking connect with 200 ms select.
        int flags = ::fcntl(s, F_GETFL, 0);
        ::fcntl(s, F_SETFL, flags | O_NONBLOCK);

        int rc = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        bool connected = (rc == 0);
        if (rc < 0 && errno == EINPROGRESS) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(s, &wfds);
            timeval tv{0, 200000};
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
        if (connected) {
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
