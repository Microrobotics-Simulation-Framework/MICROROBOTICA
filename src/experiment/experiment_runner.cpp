#include "experiment/experiment_runner.h"
#include <spdlog/spdlog.h>

namespace microbotica::experiment {

ExperimentRunner::ExperimentRunner(QObject* parent)
    : QObject(parent)
{
    MBCA_EXPERIMENTAL_WARN("ExperimentRunner");
}

ExperimentRunner::~ExperimentRunner() {
    stop();
}

bool ExperimentRunner::start(const std::string& experiment_dir) {
    if (process_ && process_->state() != QProcess::NotRunning) {
        spdlog::warn("ExperimentRunner: Process already running — stop it first");
        return false;
    }

    intentional_stop_ = false;
    status_ = RunnerStatus::Starting;

    process_ = new QProcess(this);
    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ExperimentRunner::onProcessFinished);
    connect(process_, &QProcess::started,
            this, &ExperimentRunner::onProcessStarted);

    QStringList args;
    args << "-m" << "mime.runner" << QString::fromStdString(experiment_dir);

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
