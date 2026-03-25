#pragma once

#include <string>
#include <QWidget>
#include "connection/connection_config.h"

namespace microbotica::experiment {

/// Result of the "No MIME detected" dialog.
struct ConnectionChoice {
    enum Action {
        LaunchLocal,    ///< Spawn MIME subprocess locally
        ManualEndpoint, ///< User entered a manual endpoint
        LaunchCloud,    ///< Launch via SkyPilot
        Cancelled       ///< User cancelled
    };

    Action action = Cancelled;
    std::string endpoint;  ///< Populated only for ManualEndpoint
};

/// Show the "No MIME process detected" three-button dialog.
///
/// This is a lightweight QMessageBox-based dialog, not a full settings panel.
/// The full connection settings panel is Phase F (run configuration panel).
///
/// @param parent  Parent widget for dialog positioning
/// @param has_cloud_config  If true, show "Launch on cloud" button
/// @return User's connection choice
ConnectionChoice showConnectionDialog(QWidget* parent, bool has_cloud_config);

} // namespace microbotica::experiment
