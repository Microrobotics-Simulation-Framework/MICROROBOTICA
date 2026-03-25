#include "connection/ssh_tunnel.h"
#include <spdlog/spdlog.h>

namespace microbotica::connection {

SshTunnel::SshTunnel(QObject* parent)
    : QObject(parent)
{
    MBCA_EXPERIMENTAL_WARN("SshTunnel");
}

SshTunnel::~SshTunnel() {
    close();
}

bool SshTunnel::open(const std::string& host,
                      int ssh_port,
                      const std::vector<int>& local_ports,
                      const std::vector<int>& remote_ports,
                      const std::string& user)
{
    if (process_ && process_->state() != QProcess::NotRunning) {
        spdlog::warn("SshTunnel: Tunnel already open — close it first");
        return false;
    }

    if (local_ports.size() != remote_ports.size()) {
        spdlog::error("SshTunnel: local_ports and remote_ports must have the same size");
        return false;
    }

    // Build SSH command: ssh -N -L <local>:localhost:<remote> ... -p <port> user@host
    QStringList args;
    args << "-N"; // No remote command — tunnel only
    args << "-o" << "StrictHostKeyChecking=no";
    args << "-o" << "UserKnownHostsFile=/dev/null";
    args << "-o" << "ServerAliveInterval=10";
    args << "-o" << "ServerAliveCountMax=3";

    for (size_t i = 0; i < local_ports.size(); ++i) {
        args << "-L" << QString("%1:localhost:%2")
                           .arg(local_ports[i])
                           .arg(remote_ports[i]);
    }

    args << "-p" << QString::number(ssh_port);
    args << QString::fromStdString(user + "@" + host);

    intentional_close_ = false;

    process_ = new QProcess(this);
    connect(process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SshTunnel::onProcessFinished);

    spdlog::info("SshTunnel: Opening tunnel to {}@{}:{}", user, host, ssh_port);
    process_->start("ssh", args);

    if (!process_->waitForStarted(5000)) {
        spdlog::error("SshTunnel: Failed to start SSH process: {}",
                       process_->errorString().toStdString());
        delete process_;
        process_ = nullptr;
        return false;
    }

    spdlog::info("SshTunnel: SSH tunnel process started (PID {})",
                 process_->processId());
    return true;
}

void SshTunnel::close() {
    if (!process_ || process_->state() == QProcess::NotRunning) {
        return;
    }

    intentional_close_ = true;
    spdlog::info("SshTunnel: Closing tunnel");
    process_->terminate();
    if (!process_->waitForFinished(3000)) {
        spdlog::warn("SshTunnel: SSH process did not terminate gracefully — killing");
        process_->kill();
        process_->waitForFinished(2000);
    }
}

bool SshTunnel::isOpen() const {
    return process_ && process_->state() == QProcess::Running;
}

void SshTunnel::onProcessFinished(int exit_code, QProcess::ExitStatus exit_status) {
    if (intentional_close_) {
        spdlog::info("SshTunnel: Tunnel closed (exit code {})", exit_code);
        return;
    }

    QString stderr_output;
    if (process_) {
        stderr_output = process_->readAllStandardError();
    }

    spdlog::warn("SshTunnel: Tunnel died unexpectedly (exit code {}, status {}): {}",
                 exit_code,
                 exit_status == QProcess::CrashExit ? "crash" : "normal",
                 stderr_output.toStdString());

    Q_EMIT tunnelDied(exit_code, stderr_output);
}

} // namespace microbotica::connection
