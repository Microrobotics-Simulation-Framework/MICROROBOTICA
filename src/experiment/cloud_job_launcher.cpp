#include "experiment/cloud_job_launcher.h"
#include <spdlog/spdlog.h>

namespace microbotica::experiment {

CloudJobLauncher::CloudJobLauncher(QObject* parent)
    : QObject(parent)
{
    MBCA_EXPERIMENTAL_WARN("CloudJobLauncher");
}

CloudJobLauncher::~CloudJobLauncher() {
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(3000);
    }
}

bool CloudJobLauncher::launch(const std::string& experiment_dir,
                               const std::string& job_config,
                               const std::string& cluster_name) {
    if (process_ && process_->state() != QProcess::NotRunning) {
        spdlog::warn("CloudJobLauncher: A launch is already in progress");
        return false;
    }

    std::string job_path = experiment_dir + "/" + job_config;

    process_ = new QProcess(this);
    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CloudJobLauncher::onLaunchFinished);

    QStringList args;
    args << "jobs" << "launch"
         << QString::fromStdString(job_path)
         << "--name" << QString::fromStdString(cluster_name)
         << "--workdir" << QString::fromStdString(experiment_dir)
         << "-y";  // non-interactive

    spdlog::info("CloudJobLauncher: Launching SkyPilot job '{}' from {}",
                 cluster_name, job_path);
    process_->start("sky", args);

    if (!process_->waitForStarted(10000)) {
        spdlog::error("CloudJobLauncher: Failed to start sky CLI: {}",
                       process_->errorString().toStdString());
        Q_EMIT launchFailed(process_->errorString());
        delete process_;
        process_ = nullptr;
        return false;
    }

    return true;
}

void CloudJobLauncher::cancel(const std::string& cluster_name) {
    QProcess cancel_proc;
    QStringList args;
    args << "jobs" << "cancel" << QString::fromStdString(cluster_name) << "-y";
    cancel_proc.start("sky", args);
    cancel_proc.waitForFinished(30000);

    spdlog::info("CloudJobLauncher: Cancelled SkyPilot job '{}'", cluster_name);
}

bool CloudJobLauncher::isLaunching() const {
    return process_ && process_->state() == QProcess::Running;
}

void CloudJobLauncher::onLaunchFinished(int exit_code, QProcess::ExitStatus /*exit_status*/) {
    if (exit_code == 0) {
        spdlog::info("CloudJobLauncher: SkyPilot job launched successfully");
        // Extract cluster name from stdout if needed
        QString stdout_output;
        if (process_) {
            stdout_output = process_->readAllStandardOutput();
        }
        Q_EMIT launchSucceeded(stdout_output.trimmed());
    } else {
        QString stderr_output;
        if (process_) {
            stderr_output = process_->readAllStandardError();
        }
        spdlog::error("CloudJobLauncher: SkyPilot launch failed (exit {}): {}",
                       exit_code, stderr_output.toStdString());
        Q_EMIT launchFailed(stderr_output);
    }
}

} // namespace microbotica::experiment
