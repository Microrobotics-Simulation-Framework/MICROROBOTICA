#pragma once

#include <QDockWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include "connection/connection_config.h"

namespace microbotica::panels {

/// Run configuration dock panel.
///
/// Mode selector (Local/Manual/Cloud), experiment directory picker,
/// launch/stop buttons, connection status indicator.
class RunConfigPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit RunConfigPanel(QWidget* parent = nullptr);

    /// Get the selected connection mode.
    connection::ConnectionMode selectedMode() const;

    /// Get the experiment directory path.
    std::string experimentDir() const;

    /// Set the experiment directory text. Does NOT emit
    /// experimentDirChanged — call this from code paths that already know
    /// the experiment is loaded (e.g. File → Open Experiment) to mirror
    /// the path into the panel without re-triggering initExperiment.
    void setExperimentDir(const std::string& path);

    /// Get the requested ResultFrame stream format (MIME v0.2 fit-up §8):
    /// "" (Auto — runner default), "json", or "binary". Passed to the
    /// MIME runner as MIME_STREAM_FORMAT when non-empty.
    std::string streamFormat() const;

    /// Target publish rate to request of the MIME runner, in frames per
    /// second. Returns 0 to mean "leave the runner at its default" — the
    /// caller should only set MIME_PUBLISH_FPS when this is > 0.
    int publishFps() const;

    /// SkyPilot cluster name to launch / connect to in Cloud mode.
    /// Empty in Local / Manual modes.
    std::string clusterName() const;

    /// Path to the SkyPilot job YAML (relative to the experiment dir).
    /// Empty in Local / Manual modes. When empty, the launch path falls
    /// back to `cloud.job_config` from experiment.yaml.
    std::string jobConfig() const;

    /// True if, on Stop, the launched SkyPilot cluster should be torn
    /// down (`sky down`). On by default to prevent runaway cost.
    bool stopClusterOnDisconnect() const;

    /// Update connection status display.
    void setConnectionStatus(const QString& status);

    /// Enable/disable launch button.
    void setLaunchEnabled(bool enabled);

Q_SIGNALS:
    void launchRequested();
    void stopRequested();
    void experimentDirChanged(const QString& path);

private:
    QComboBox* modeCombo_ = nullptr;
    QComboBox* streamFormatCombo_ = nullptr;
    QSpinBox* publishFpsSpin_ = nullptr;
    QLineEdit* experimentDirEdit_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    // Cloud-mode fields, shown only when modeCombo_ == Cloud.
    QLabel* clusterNameLabel_ = nullptr;
    QLineEdit* clusterNameEdit_ = nullptr;
    QLabel* jobConfigLabel_ = nullptr;
    QLineEdit* jobConfigEdit_ = nullptr;
    QCheckBox* stopClusterCheck_ = nullptr;
};

} // namespace microbotica::panels
