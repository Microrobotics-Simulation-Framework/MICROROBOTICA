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
    pauseAction_->setToolTip(tr("Stop simulation (F6)"));
    pauseAction_->setEnabled(false);
    connect(pauseAction_, &QAction::triggered, this, [this]() {
        Q_EMIT stopRequested();
    });

    stopAction_ = addAction(tr("\u23F9 Stop"));
    stopAction_->setShortcut(QKeySequence(Qt::Key_F7));
    stopAction_->setToolTip(tr("Stop simulation (F7)"));
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

    addSeparator();

    // ── Camera snap buttons ──────────────────────────────────────────────
    if (controller) {
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

        // Trigger viewport update after any snap/reset
        auto triggerUpdate = [parent]() {
            if (parent) parent->update();
        };
        connect(front, &QAction::triggered, this, triggerUpdate);
        connect(back, &QAction::triggered, this, triggerUpdate);
        connect(top, &QAction::triggered, this, triggerUpdate);
        connect(bottom, &QAction::triggered, this, triggerUpdate);
        connect(left, &QAction::triggered, this, triggerUpdate);
        connect(right, &QAction::triggered, this, triggerUpdate);
        connect(reset, &QAction::triggered, this, triggerUpdate);

        // F1/? help from keyboard
        connect(controller, &CameraController::helpRequested, this, [this]() {
            showHelp(this);
        });
    }

    addSeparator();

    auto* help = addAction(tr("?"));
    help->setToolTip(tr("Viewport controls help (F1)"));
    connect(help, &QAction::triggered, this, [this]() {
        showHelp(this);
    });

    // Wire simulation state to button enable/disable
    if (simController_) {
        connect(simController_, &simulation::SimulationController::simulationStarted,
                this, &CameraToolbar::onSimStarted);
        connect(simController_, &simulation::SimulationController::simulationStopped,
                this, &CameraToolbar::onSimStopped);
    }
}

void CameraToolbar::onSimStarted() {
    playAction_->setEnabled(false);
    pauseAction_->setEnabled(true);
    stopAction_->setEnabled(true);
}

void CameraToolbar::onSimStopped() {
    playAction_->setEnabled(true);
    pauseAction_->setEnabled(false);
    stopAction_->setEnabled(false);
}

void CameraToolbar::showHelp(QWidget* parent) {
    QMessageBox::information(parent, tr("Viewport Controls"),
        tr("<h3>Simulation</h3>"
           "<table cellpadding='4'>"
           "<tr><td><b>F5</b></td><td>Start simulation</td></tr>"
           "<tr><td><b>F6 / F7</b></td><td>Stop simulation</td></tr>"
           "<tr><td><b>F11</b></td><td>Toggle fullscreen viewport</td></tr>"
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
