#include "app/main_window.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>

#include "viewport/viewport_widget.h"
#include "panels/scene_hierarchy_panel.h"
#include "panels/property_panel.h"
#include "panels/timeline_panel.h"
#include "panels/console_widget.h"
#include "stubs/local_compute_backend.h"

namespace microbotica::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    MBCA_EXPERIMENTAL_WARN("MainWindow");

    setWindowTitle(tr("MICROBOTICA"));
    resize(1600, 900);

    // Create core services
    sceneMgr_ = std::make_unique<scene::SceneManager>();
    primSelection_ = std::make_unique<scene::PrimSelection>();
    simController_ = std::make_unique<simulation::SimulationController>(*sceneMgr_);
    scriptEngine_ = std::make_unique<scripting::ScriptingEngine>();
    scriptEngine_->initialize();

    // Central viewport
    viewportWidget_ = new viewport::ViewportWidget(this);
    setCentralWidget(viewportWidget_);

    createDockWidgets();
    createMenuBar();
    createStatusBar();
    wireSignals();
}

MainWindow::~MainWindow()
{
    // Ensure simulation is stopped before services are destroyed
    if (simController_ && simController_->isRunning()) {
        simController_->teardown();
    }
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void MainWindow::createMenuBar()
{
    // File menu
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    auto* openAction = fileMenu->addAction(tr("&Open USD Scene..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onFileOpen);

    auto* closeAction = fileMenu->addAction(tr("&Close Scene"));
    closeAction->setShortcut(QKeySequence::Close);
    connect(closeAction, &QAction::triggered, this, &MainWindow::onFileClose);

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // Simulation menu
    auto* simMenu = menuBar()->addMenu(tr("&Simulation"));

    auto* startAction = simMenu->addAction(tr("&Start"));
    connect(startAction, &QAction::triggered, this, &MainWindow::onSimulationStart);

    auto* stopAction = simMenu->addAction(tr("S&top"));
    connect(stopAction, &QAction::triggered, this, &MainWindow::onSimulationStop);

    // Help menu
    auto* helpMenu = menuBar()->addMenu(tr("&Help"));

    auto* aboutAction = helpMenu->addAction(tr("&About MICROBOTICA"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void MainWindow::createStatusBar()
{
    simTimeLabel_ = new QLabel(tr("Sim: 0.000 s"), this);
    statusBar()->addPermanentWidget(simTimeLabel_);
    statusBar()->showMessage(tr("Ready"));
}

// ---------------------------------------------------------------------------
// Dock widgets
// ---------------------------------------------------------------------------

void MainWindow::createDockWidgets()
{
    // Left docks: hierarchy (top-left), property (bottom-left)
    hierarchyPanel_ = new panels::SceneHierarchyPanel(*sceneMgr_, *primSelection_, this);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyPanel_);

    propertyPanel_ = new panels::PropertyPanel(*primSelection_, this);
    addDockWidget(Qt::LeftDockWidgetArea, propertyPanel_);

    // Stack hierarchy above property on the left
    splitDockWidget(hierarchyPanel_, propertyPanel_, Qt::Vertical);

    // Bottom docks: timeline + console
    timelinePanel_ = new panels::TimelinePanel(*simController_, this);
    addDockWidget(Qt::BottomDockWidgetArea, timelinePanel_);

    consoleWidget_ = new panels::ConsoleWidget(*scriptEngine_, this);
    addDockWidget(Qt::BottomDockWidgetArea, consoleWidget_);

    // Tab timeline and console together at the bottom
    tabifyDockWidget(timelinePanel_, consoleWidget_);
    timelinePanel_->raise(); // Show timeline tab by default
}

// ---------------------------------------------------------------------------
// Signal wiring
// ---------------------------------------------------------------------------

void MainWindow::wireSignals()
{
    // Scene loaded → set stage on viewport
    connect(sceneMgr_.get(), &scene::SceneManager::sceneLoaded, this, [this]() {
#ifdef MICROBOTICA_HAS_USD
        viewportWidget_->setStage(sceneMgr_->stage());
#endif
        statusBar()->showMessage(tr("Scene loaded"));
    });

    // Simulation frame ready → apply to scene and update status bar
    connect(simController_.get(), &simulation::SimulationController::frameReady,
            this, &MainWindow::onFrameReady);

    // Backend crash
    connect(simController_.get(), &simulation::SimulationController::backendCrashed,
            this, &MainWindow::onBackendCrashed);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::onFileOpen()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open USD Scene"),
        QString(),
        tr("USD Files (*.usd *.usda *.usdc *.usdz);;All Files (*)"));

    if (filePath.isEmpty()) return;

    const bool ok = sceneMgr_->loadScene(filePath.toStdString());
    if (!ok) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not open scene file:\n%1").arg(filePath));
    }
}

void MainWindow::onFileClose()
{
    if (simController_->isRunning()) {
        simController_->stop();
    }
    // SceneManager does not have an explicit close — for Phase 0 this is a no-op.
    primSelection_->clear();
    statusBar()->showMessage(tr("Scene closed"));
}

void MainWindow::onSimulationStart()
{
    if (!sceneMgr_->isLoaded()) {
        QMessageBox::information(this, tr("No Scene"),
                                 tr("Please open a USD scene before starting simulation."));
        return;
    }

    if (simController_->isRunning()) return;

    // Create a local compute backend and launch with default config
    auto backend = std::make_unique<stubs::LocalComputeBackend>();
    simController_->setComputeBackend(std::move(backend));

    core::PhysicsConfig config;
    simController_->launchPhysics(config);

    statusBar()->showMessage(tr("Simulation started"));
}

void MainWindow::onSimulationStop()
{
    simController_->stop();
    statusBar()->showMessage(tr("Simulation stopped"));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About MICROBOTICA"),
        tr("<h3>MICROBOTICA v0.1.0</h3>"
           "<p>Medical microrobotics simulation platform.</p>"
           "<p>Phase 0 — Experimental</p>"
           "<p>Licensed under AGPL-3.0</p>"));
}

void MainWindow::onFrameReady(const core::ResultFrame& frame)
{
    sceneMgr_->applyResultFrame(frame, simController_->physicsConfig());

    const double t = frame.simTime;
    simTimeLabel_->setText(QString("Sim: %1 s").arg(t, 0, 'f', 3));
}

void MainWindow::onBackendCrashed(const QString& reason)
{
    statusBar()->showMessage(tr("Backend crashed: %1").arg(reason));
    QMessageBox::critical(this, tr("Backend Crashed"),
        tr("The simulation backend has crashed:\n%1\n\n"
           "The results layer will be cleared.").arg(reason));
    sceneMgr_->crashRecovery();
}

} // namespace microbotica::app
