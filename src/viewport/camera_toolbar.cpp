#include "viewport/camera_toolbar.h"
#include "viewport/camera_controller.h"

#include <QAction>
#include <QMessageBox>

namespace microbotica::viewport {

CameraToolbar::CameraToolbar(CameraController* controller, QWidget* parent)
    : QToolBar(tr("Camera"), parent)
{
    setObjectName("CameraToolbar");
    setIconSize(QSize(16, 16));

    // Axis-snap buttons
    auto* front = addAction(tr("Front"));
    front->setToolTip(tr("Front view (1)"));
    connect(front, &QAction::triggered, controller, &CameraController::snapFront);

    auto* back = addAction(tr("Back"));
    back->setToolTip(tr("Back view (2)"));
    connect(back, &QAction::triggered, controller, &CameraController::snapBack);

    auto* top = addAction(tr("Top"));
    top->setToolTip(tr("Top view (3)"));
    connect(top, &QAction::triggered, controller, &CameraController::snapTop);

    auto* bottom = addAction(tr("Bottom"));
    bottom->setToolTip(tr("Bottom view (4)"));
    connect(bottom, &QAction::triggered, controller, &CameraController::snapBottom);

    auto* left = addAction(tr("Left"));
    left->setToolTip(tr("Left view (5)"));
    connect(left, &QAction::triggered, controller, &CameraController::snapLeft);

    auto* right = addAction(tr("Right"));
    right->setToolTip(tr("Right view (6)"));
    connect(right, &QAction::triggered, controller, &CameraController::snapRight);

    addSeparator();

    auto* reset = addAction(tr("Reset"));
    reset->setToolTip(tr("Reset camera to default (R / Home)"));
    connect(reset, &QAction::triggered, controller, &CameraController::resetToDefault);

    addSeparator();

    auto* help = addAction(tr("?"));
    help->setToolTip(tr("Viewport controls help (F1)"));
    connect(help, &QAction::triggered, this, [this]() {
        showHelp(this);
    });

    // Also connect the keyboard F1/? shortcut from controller
    connect(controller, &CameraController::helpRequested, this, [this]() {
        showHelp(this);
    });

    // Trigger viewport update after any snap/reset
    auto triggerUpdate = [controller, parent]() {
        if (parent) parent->update();
    };
    connect(front, &QAction::triggered, this, triggerUpdate);
    connect(back, &QAction::triggered, this, triggerUpdate);
    connect(top, &QAction::triggered, this, triggerUpdate);
    connect(bottom, &QAction::triggered, this, triggerUpdate);
    connect(left, &QAction::triggered, this, triggerUpdate);
    connect(right, &QAction::triggered, this, triggerUpdate);
    connect(reset, &QAction::triggered, this, triggerUpdate);
}

void CameraToolbar::showHelp(QWidget* parent) {
    QMessageBox::information(parent, tr("Viewport Controls"),
        tr("<h3>Camera Controls</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>Left drag</b></td><td>Orbit (rotate around scene)</td></tr>"
           "<tr><td><b>Alt + Left drag</b></td><td>Orbit (alternative)</td></tr>"
           "<tr><td><b>Shift + Left drag</b></td><td>Pan</td></tr>"
           "<tr><td><b>Right drag</b></td><td>Pan</td></tr>"
           "<tr><td><b>Middle drag</b></td><td>Pan</td></tr>"
           "<tr><td><b>Scroll wheel</b></td><td>Zoom in/out</td></tr>"
           "<tr><td><b>+ / -</b></td><td>Zoom in/out</td></tr>"
           "</table>"
           "<h3>Keyboard Navigation</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>Arrow keys</b></td><td>Orbit in small steps</td></tr>"
           "<tr><td><b>W / A / S / D</b></td><td>Pan</td></tr>"
           "<tr><td><b>R / Home</b></td><td>Reset camera</td></tr>"
           "</table>"
           "<h3>Axis-Snap Views</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>1</b></td><td>Front</td></tr>"
           "<tr><td><b>2</b></td><td>Back</td></tr>"
           "<tr><td><b>3</b></td><td>Top</td></tr>"
           "<tr><td><b>4</b></td><td>Bottom</td></tr>"
           "<tr><td><b>5</b></td><td>Left</td></tr>"
           "<tr><td><b>6</b></td><td>Right</td></tr>"
           "</table>"
           "<p><i>Click inside the viewport first to ensure it has focus.</i></p>"));
}

} // namespace microbotica::viewport
