#include "viewport/camera_toolbar.h"
#include "viewport/camera_controller.h"
#include "simulation/simulation_controller.h"

#include <QAction>
#include <QMessageBox>

namespace microbotica::viewport {

CameraToolbar::CameraToolbar(CameraController* controller,
                               simulation::SimulationController* simController,
                               QWidget* parent)
    : QToolBar(tr("Viewport"), parent)
    , simController_(simController)
{
    setObjectName("CameraToolbar");
    setIconSize(QSize(16, 16));

    // ── Simulation controls ──────────────────────────────────────────────
    playAction_ = addAction(tr("\u25B6 Play"));
    playAction_->setShortcut(QKeySequence(Qt::Key_F5));
    playAction_->setToolTip(tr("Start simulation (F5)"));
    connect(playAction_, &QAction::triggered, this, [this]() {
        Q_EMIT playRequested();
    });

    pauseAction_ = addAction(tr("\u23F8 Pause"));
    pauseAction_->setShortcut(QKeySequence(Qt::Key_F6));
    pauseAction_->setToolTip(tr("Pause simulation — resume with Play (F6)"));
    pauseAction_->setEnabled(false);
    connect(pauseAction_, &QAction::triggered, this, [this]() {
        Q_EMIT pauseRequested();
    });

    stopAction_ = addAction(tr("\u23F9 Stop"));
    stopAction_->setShortcut(QKeySequence(Qt::Key_F7));
    stopAction_->setToolTip(tr("Stop and reset simulation (F7)"));
    stopAction_->setEnabled(false);
    connect(stopAction_, &QAction::triggered, this, [this]() {
        Q_EMIT stopRequested();
    });

    addSeparator();

    // ── Fullscreen toggle ────────────────────────────────────────────────
    auto* fullscreen = addAction(tr("\u26F6 Fullscreen"));
    fullscreen->setShortcut(QKeySequence(Qt::Key_F11));
    fullscreen->setToolTip(tr("Toggle fullscreen viewport (F11)"));
    connect(fullscreen, &QAction::triggered, this, &CameraToolbar::fullscreenToggled);

    // ── Camera actions insert point ──────────────────────────────────────
    cameraSeparator_ = addSeparator();

    // ── Help ─────────────────────────────────────────────────────────────
    auto* help = addAction(tr("?"));
    help->setToolTip(tr("Viewport controls help (F1)"));
    connect(help, &QAction::triggered, this, [this]() {
        showHelp(this);
    });

    // Wire simulation state
    if (simController_) {
        connect(simController_, &simulation::SimulationController::simulationStarted,
                this, &CameraToolbar::onSimStarted);
        connect(simController_, &simulation::SimulationController::simulationStopped,
                this, &CameraToolbar::onSimStopped);
    }

    // If a controller was provided at construction, wire it now
    if (controller) {
        setCameraController(controller);
    }
}

void CameraToolbar::setCameraController(CameraController* controller) {
    // Remove any previous camera actions
    for (auto* action : cameraActions_) {
        removeAction(action);
        delete action;
    }
    cameraActions_.clear();
    cameraController_ = controller;

    if (!controller) return;

    // Insert camera actions before the help separator
    auto addCamAction = [this](const QString& text, const QString& tip,
                                void (CameraController::*slot)()) {
        auto* action = new QAction(text, this);
        action->setToolTip(tip);
        connect(action, &QAction::triggered, cameraController_, slot);
        connect(action, &QAction::triggered, this, [this]() {
            if (parentWidget()) parentWidget()->update();
        });
        insertAction(cameraSeparator_, action);
        cameraActions_.append(action);
        return action;
    };

    auto* sep1 = new QAction(this);
    sep1->setSeparator(true);
    insertAction(cameraSeparator_, sep1);
    cameraActions_.append(sep1);

    addCamAction(tr("Front"),  tr("Front view (1)"),  &CameraController::snapFront);
    addCamAction(tr("Back"),   tr("Back view (2)"),   &CameraController::snapBack);
    addCamAction(tr("Top"),    tr("Top view (3)"),    &CameraController::snapTop);
    addCamAction(tr("Bottom"), tr("Bottom view (4)"), &CameraController::snapBottom);
    addCamAction(tr("Left"),   tr("Left view (5)"),   &CameraController::snapLeft);
    addCamAction(tr("Right"),  tr("Right view (6)"),  &CameraController::snapRight);

    auto* sep2 = new QAction(this);
    sep2->setSeparator(true);
    insertAction(cameraSeparator_, sep2);
    cameraActions_.append(sep2);

    addCamAction(tr("Reset"),  tr("Reset camera (R / Home)"), &CameraController::resetToDefault);

    connect(controller, &CameraController::helpRequested, this, [this]() {
        showHelp(this);
    });
}

void CameraToolbar::onSimStarted() {
    playAction_->setEnabled(false);
    playAction_->setText(tr("\u25B6 Play"));
    pauseAction_->setEnabled(true);
    stopAction_->setEnabled(true);
}

void CameraToolbar::onSimStopped() {
    playAction_->setEnabled(true);
    playAction_->setText(tr("\u25B6 Play"));
    pauseAction_->setEnabled(false);
    stopAction_->setEnabled(false);
}

void CameraToolbar::onSimPaused() {
    playAction_->setEnabled(true);
    playAction_->setText(tr("\u25B6 Resume"));
    pauseAction_->setEnabled(false);
    stopAction_->setEnabled(true);
}

void CameraToolbar::showHelp(QWidget* parent) {
    QMessageBox::information(parent, tr("Viewport Controls"),
        tr("<h3>Simulation</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>F5</b></td><td>Start / Resume</td></tr>"
           "<tr><td><b>F6</b></td><td>Pause</td></tr>"
           "<tr><td><b>F7</b></td><td>Stop</td></tr>"
           "<tr><td><b>F11</b></td><td>Toggle fullscreen</td></tr>"
           "</table>"
           "<h3>Camera — Mouse</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>Left drag</b></td><td>Orbit (rotate)</td></tr>"
           "<tr><td><b>Shift + Left drag</b></td><td>Pan</td></tr>"
           "<tr><td><b>Right drag</b></td><td>Pan</td></tr>"
           "<tr><td><b>Scroll wheel</b></td><td>Zoom</td></tr>"
           "</table>"
           "<h3>Camera — Keyboard</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>Arrow keys</b></td><td>Orbit</td></tr>"
           "<tr><td><b>W / A / S / D</b></td><td>Pan</td></tr>"
           "<tr><td><b>+ / -</b></td><td>Zoom</td></tr>"
           "<tr><td><b>R / Home</b></td><td>Reset camera</td></tr>"
           "</table>"
           "<h3>Axis-Snap Views</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>1</b></td><td>Front</td>"
           "<td><b>2</b></td><td>Back</td></tr>"
           "<tr><td><b>3</b></td><td>Top</td>"
           "<td><b>4</b></td><td>Bottom</td></tr>"
           "<tr><td><b>5</b></td><td>Left</td>"
           "<td><b>6</b></td><td>Right</td></tr>"
           "</table>"
           "<p><i>Click the viewport first to give it keyboard focus.</i></p>"));
}

} // namespace microbotica::viewport
