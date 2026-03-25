#pragma once

#include <string>
#include <QObject>
#include <QProcess>
#include "core/component_meta.h"
#include "core/stability.h"

namespace microbotica::experiment {

/// Launches SkyPilot cloud jobs for experiment execution.
///
/// MBCA-COMP-062
///
/// Reads the cloud.job_config from experiment.yaml and invokes
/// `sky jobs launch` via QProcess. The job YAML format is the same
/// as production_h100.yaml / rehearsal_a100.yaml already in use.
class CloudJobLauncher : public QObject {
    Q_OBJECT

public:
    explicit CloudJobLauncher(QObject* parent = nullptr);
    ~CloudJobLauncher() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-062",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Launches SkyPilot cloud jobs for remote experiment execution.",
            .preconditions     = {"SkyPilot CLI (sky) is installed and configured",
                                  "Cloud credentials are available (RunPod API key, etc.)"},
            .postconditions    = {"launch() submits a SkyPilot job and returns the cluster name",
                                  "cancel() terminates the running job"},
            .invariants        = {},
            .design_rationale  = "Uses the existing SkyPilot job YAML format (same as production sweeps) "
                                 "to avoid introducing a new cloud config format.",
            .assumptions       = {"SkyPilot manages SSH keys and instance provisioning",
                                  "The experiment directory is synced to the instance via workdir"},
            .limitations       = {"No progress monitoring — only launch and cancel",
                                  "No cost tracking beyond SkyPilot's autostop"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Launching a cloud job incurs real monetary cost. The job_config should "
                "include max_total_budget and autostop_minutes as safety nets.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Launch a SkyPilot job.
    /// @param experiment_dir  Absolute path to the experiment directory
    /// @param job_config      Relative path to the SkyPilot job YAML within experiment_dir
    /// @param cluster_name    Desired SkyPilot cluster name
    /// @return true if the launch process started
    bool launch(const std::string& experiment_dir,
                const std::string& job_config,
                const std::string& cluster_name);

    /// Cancel a running SkyPilot job.
    /// @param cluster_name  SkyPilot cluster name to cancel
    void cancel(const std::string& cluster_name);

    /// @return true if a launch is in progress
    bool isLaunching() const;

Q_SIGNALS:
    void launchSucceeded(const QString& cluster_name);
    void launchFailed(const QString& error);

private Q_SLOTS:
    void onLaunchFinished(int exit_code, QProcess::ExitStatus exit_status);

private:
    QProcess* process_ = nullptr;
};

} // namespace microbotica::experiment
