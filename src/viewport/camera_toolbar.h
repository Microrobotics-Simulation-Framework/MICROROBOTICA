#pragma once

#include <QToolBar>

namespace microbotica::viewport {

class CameraController;

/// Toolbar with camera snap-to-axis buttons, reset, and help.
///
/// Placed above the viewport. Provides mouse-accessible alternatives
/// to the keyboard shortcuts for laptop/trackpad users.
class CameraToolbar : public QToolBar {
    Q_OBJECT

public:
    CameraToolbar(CameraController* controller, QWidget* parent = nullptr);

    /// Show the viewport controls help dialog.
    static void showHelp(QWidget* parent);
};

} // namespace microbotica::viewport
