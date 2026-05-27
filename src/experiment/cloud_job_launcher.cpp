#include "experiment/cloud_job_launcher.h"
#include <spdlog/spdlog.h>
#include <filesystem>

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

    process_ = new QProcess(this);
    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CloudJobLauncher::onLaunchFinished);

    const QStringList args =
        buildLaunchArgs(experiment_dir, job_config, cluster_name);

    spdlog::info("CloudJobLauncher: sky {}",
                 args.join(QLatin1Char(' ')).toStdString());
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

QStringList CloudJobLauncher::buildLaunchArgs(
        const std::string& experiment_dir,
        const std::string& job_config,
        const std::string& cluster_name) {
    // `sky launch -c <name> -y -d` (NOT `sky jobs launch`) — we want a
    // persistent cluster we can resolve, tunnel to, and keep talking to;
    // managed jobs would tear themselves down on completion. `-d`
    // (detach-run) returns once the task is dispatched, leaving the
    // cluster up.

    // Canonicalize the workdir. When MICROROBOTICA runs inside the
    // cloud-dev container, the sky API daemon is on the host (with the
    // host's ~/.sky bind-mounted in), so it resolves --workdir against
    // the host filesystem. `run-cloud-dev.sh` symlinks /workspace/*
    // into /home/nick/MSF/msf/* (host-symmetric paths bind-mounted
    // through), so canonical() turns a container-local path the user
    // picked in QFileDialog into something sky on the host can stat.
    std::string resolved = experiment_dir;
    try {
        resolved = std::filesystem::canonical(experiment_dir).string();
    } catch (const std::filesystem::filesystem_error& e) {
        spdlog::warn("CloudJobLauncher: canonical('{}') failed ({}), "
                     "using path as-is", experiment_dir, e.what());
    }

    const std::string job_path = resolved + "/" + job_config;
    QStringList args;
    args << QStringLiteral("launch")
         << QStringLiteral("-y")
         << QStringLiteral("-d")
         << QStringLiteral("-c") << QString::fromStdString(cluster_name)
         << QStringLiteral("--workdir") << QString::fromStdString(resolved)
         << QString::fromStdString(job_path);
    return args;
}

void CloudJobLauncher::cancel(const std::string& cluster_name) {
    // For interactive `sky launch -c <name>` clusters, the cleanup is
    // `sky down -y <name>`. Run it detached because tearing down a SkyPilot
    // cluster can take 30+ seconds and we don't want to block the UI Stop
    // path on it.
    QStringList args;
    args << "down" << "-y" << QString::fromStdString(cluster_name);
    QProcess::startDetached("sky", args);
    spdlog::info("CloudJobLauncher: requested `sky down -y {}` (detached)",
                 cluster_name);
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
