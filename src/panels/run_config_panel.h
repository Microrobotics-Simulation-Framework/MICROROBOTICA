#pragma once

#include <QDockWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
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
    QLineEdit* experimentDirEdit_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

} // namespace microbotica::panels
