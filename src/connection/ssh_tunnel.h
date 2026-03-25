#pragma once

#include <string>
#include <functional>
#include <QProcess>
#include <QObject>
#include "core/component_meta.h"
#include "core/stability.h"

namespace microbotica::connection {

/// Manages an SSH tunnel process for cloud mode ZMQ connections.
///
/// MBCA-COMP-051
///
/// Opens an SSH tunnel binding remote ZMQ ports to localhost via QProcess.
/// Monitors the process for unexpected exits (spot preemption, network failure).
/// Emits tunnelDied() when the SSH process terminates, allowing ConnectionManager
/// to re-resolve and reopen.
class SshTunnel : public QObject {
    Q_OBJECT

public:
    explicit SshTunnel(QObject* parent = nullptr);
    ~SshTunnel() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-051",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "SSH tunnel process manager for cloud mode ZMQ connections.",
            .preconditions     = {"SSH key authentication is configured for the target host",
                                  "Remote host is reachable on the specified SSH port"},
            .postconditions    = {"open() starts an SSH tunnel binding remote ports to localhost",
                                  "close() terminates the tunnel process",
                                  "tunnelDied signal emitted on unexpected process termination"},
            .invariants        = {"At most one tunnel process is active at a time"},
            .design_rationale  = "SSH tunnel reuses SkyPilot's existing SSH key infrastructure, "
                                 "keeps ZMQ endpoint always localhost, works through firewalls.",
            .assumptions       = {"SkyPilot has provisioned SSH access to the cloud instance",
                                  "Port forwarding is permitted on the SSH connection"},
            .limitations       = {"No multiplexing — one tunnel per SshTunnel instance",
                                  "No automatic retry — caller must re-open on tunnelDied"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Spot preemption kills the remote host, causing the SSH process to exit. "
                "The tunnelDied signal is the primary detection mechanism.",
                "SSH process may take up to TCP keepalive timeout (~30s) to detect a dead host.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Open an SSH tunnel.
    /// @param host  Remote IP address
    /// @param ssh_port  Remote SSH port
    /// @param local_ports  Local ports to forward (e.g., {5555, 5556})
    /// @param remote_ports  Corresponding remote ports
    /// @param user  SSH user (default "root" for SkyPilot instances)
    /// @return true if the process started successfully
    bool open(const std::string& host,
              int ssh_port,
              const std::vector<int>& local_ports,
              const std::vector<int>& remote_ports,
              const std::string& user = "root");

    /// Close the tunnel process gracefully.
    void close();

    /// @return true if the tunnel process is running
    bool isOpen() const;

Q_SIGNALS:
    /// Emitted when the SSH tunnel process terminates unexpectedly.
    /// @param exit_code  Process exit code
    /// @param error_output  stderr from the SSH process
    void tunnelDied(int exit_code, const QString& error_output);

private Q_SLOTS:
    void onProcessFinished(int exit_code, QProcess::ExitStatus exit_status);

private:
    QProcess* process_ = nullptr;
    bool intentional_close_ = false;
};

} // namespace microbotica::connection
