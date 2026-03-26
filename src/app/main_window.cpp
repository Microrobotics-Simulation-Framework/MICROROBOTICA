#include "app/main_window.h"

#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QVBoxLayout>

#include "app/settings_dialog.h"
#include "app/theme_manager.h"
#include "viewport/viewport_widget.h"
#include "panels/scene_hierarchy_panel.h"
#include "panels/property_panel.h"
#include "panels/timeline_panel.h"
#include "panels/console_widget.h"
#include "panels/simulation_toolbar.h"
#include "panels/mime_console_panel.h"
#include "panels/run_config_panel.h"
#include "panels/skypilot_monitor_panel.h"
#include "panels/project_browser_panel.h"
#include "panels/graph_inspector_panel.h"
#include "panels/script_editor_panel.h"
#include "panels/parameter_panel.h"
#include "viewport/camera_toolbar.h"
#include "viewport/camera_controller.h"
#include "viewport/local_viewport.h"
#include "stubs/local_compute_backend.h"

namespace microbotica::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    MBCA_EXPERIMENTAL_WARN("MainWindow");

    setWindowTitle(tr("MICROBOTICA"));
    resize(1600, 900);

    // Apply theme before creating widgets
    ThemeManager::applyTheme(ThemeManager::currentTheme());

    // Create core services
    sceneMgr_ = std::make_unique<scene::SceneManager>();
    primSelection_ = std::make_unique<scene::PrimSelection>();
    simController_ = std::make_unique<simulation::SimulationController>(*sceneMgr_);
    scriptEngine_ = std::make_unique<scripting::ScriptingEngine>();
    scriptEngine_->initialize();

    // Central viewport with camera toolbar above it
    viewportWidget_ = new viewport::ViewportWidget(this);

    auto* viewportContainer = new QWidget(this);
    auto* viewportLayout = new QVBoxLayout(viewportContainer);
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->setSpacing(0);

    // Camera toolbar will be created after viewport is set up (needs camera controller)
    // Add viewport first, toolbar is prepended in createToolBars()
    viewportLayout->addWidget(viewportWidget_);
    setCentralWidget(viewportContainer);

    // Bottom-left corner goes to left dock area (hierarchy stays on left)
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);

    createToolBars();
    createDockWidgets();
    createMenuBar();
    createStatusBar();
    wireSignals();

    // Capture factory layout after all widgets are created
    factoryLayout_ = saveState();
    factoryGeometry_ = saveGeometry();

    // Restore user layout (overrides factory if saved)
    restoreLayout();
}

MainWindow::~MainWindow()
{
    if (simController_ && simController_->isRunning()) {
        simController_->teardown();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveLayout();
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
// Toolbars
// ---------------------------------------------------------------------------

void MainWindow::createToolBars() {
    simToolbar_ = new panels::SimulationToolbar(*simController_, this);
    addToolBar(Qt::TopToolBarArea, simToolbar_);

    // The camera toolbar needs a CameraController. Since LocalViewport is
    // created lazily, we create the toolbar now but wire it when the
    // viewport activates. For now, create with nullptr — the toolbar buttons
    // will be connected in wireSignals via renderModeChanged.
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

    auto* settingsAction = fileMenu->addAction(tr("Se&ttings..."));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

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

    // View menu
    auto* viewMenu = menuBar()->addMenu(tr("&View"));

    auto* resetLayoutAction = viewMenu->addAction(tr("&Reset Layout"));
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::onResetLayout);

    viewMenu->addSeparator();

    // Add dock toggle actions
    viewMenu->addAction(hierarchyPanel_->toggleViewAction());
    viewMenu->addAction(propertyPanel_->toggleViewAction());
    viewMenu->addAction(timelinePanel_->toggleViewAction());
    viewMenu->addAction(consoleWidget_->toggleViewAction());
    viewMenu->addAction(mimeConsole_->toggleViewAction());
    viewMenu->addAction(runConfigPanel_->toggleViewAction());
    viewMenu->addAction(skypilotPanel_->toggleViewAction());
    viewMenu->addAction(projectBrowser_->toggleViewAction());
    viewMenu->addAction(graphInspector_->toggleViewAction());
    viewMenu->addAction(scriptEditor_->toggleViewAction());
    viewMenu->addAction(parameterPanel_->toggleViewAction());

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
    splitDockWidget(hierarchyPanel_, propertyPanel_, Qt::Vertical);

    // Right docks: run config (top-right), project browser (bottom-right)
    runConfigPanel_ = new panels::RunConfigPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, runConfigPanel_);

    projectBrowser_ = new panels::ProjectBrowserPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, projectBrowser_);
    splitDockWidget(runConfigPanel_, projectBrowser_, Qt::Vertical);

    // Bottom docks: timeline, console, MIME console, SkyPilot monitor
    timelinePanel_ = new panels::TimelinePanel(*simController_, this);
    addDockWidget(Qt::BottomDockWidgetArea, timelinePanel_);

    consoleWidget_ = new panels::ConsoleWidget(*scriptEngine_, this);
    addDockWidget(Qt::BottomDockWidgetArea, consoleWidget_);

    mimeConsole_ = new panels::MimeConsolePanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, mimeConsole_);

    // scripts_dir for sky_resolve.py — relative to application binary or source
    std::string scripts_dir = ".";  // Will be resolved at runtime
    skypilotPanel_ = new panels::SkyPilotMonitorPanel(scripts_dir, this);
    addDockWidget(Qt::BottomDockWidgetArea, skypilotPanel_);

    // Phase G panels
    graphInspector_ = new panels::GraphInspectorPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, graphInspector_);

    scriptEditor_ = new panels::ScriptEditorPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, scriptEditor_);

    parameterPanel_ = new panels::ParameterPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, parameterPanel_);

    // Tab bottom panels together
    tabifyDockWidget(timelinePanel_, consoleWidget_);
    tabifyDockWidget(consoleWidget_, mimeConsole_);
    tabifyDockWidget(mimeConsole_, skypilotPanel_);
    tabifyDockWidget(skypilotPanel_, graphInspector_);
    tabifyDockWidget(graphInspector_, scriptEditor_);
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

    // Run config panel signals
    connect(runConfigPanel_, &panels::RunConfigPanel::launchRequested,
            this, &MainWindow::onSimulationStart);
    connect(runConfigPanel_, &panels::RunConfigPanel::stopRequested,
            this, &MainWindow::onSimulationStop);

    // Project browser → script editor (open file on double-click)
    connect(projectBrowser_, &panels::ProjectBrowserPanel::fileDoubleClicked,
            this, [this](const QString& path) {
        if (path.endsWith(".py") || path.endsWith(".yaml") || path.endsWith(".yml")) {
            scriptEditor_->openFile(path.toStdString());
            scriptEditor_->raise();
        }
    });

    // Simulation state → run config panel + viewport continuous rendering
    connect(simController_.get(), &simulation::SimulationController::simulationStarted,
            this, [this]() {
        runConfigPanel_->setLaunchEnabled(false);
        runConfigPanel_->setConnectionStatus(tr("Running"));
        viewportWidget_->setContinuousRendering(true);
    });
    connect(simController_.get(), &simulation::SimulationController::simulationStopped,
            this, [this]() {
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Idle"));
        viewportWidget_->setContinuousRendering(false);
    });
}

// ---------------------------------------------------------------------------
// Layout persistence
// ---------------------------------------------------------------------------

void MainWindow::saveLayout() {
    QSettings s;
    s.beginGroup("mainwindow");
    s.setValue("geometry", saveGeometry());
    s.setValue("state", saveState());
    s.endGroup();
}

void MainWindow::restoreLayout() {
    QSettings s;
    s.beginGroup("mainwindow");
    QByteArray geometry = s.value("geometry").toByteArray();
    QByteArray state = s.value("state").toByteArray();
    s.endGroup();

    if (!geometry.isEmpty()) restoreGeometry(geometry);
    if (!state.isEmpty()) restoreState(state);
}

void MainWindow::onResetLayout() {
    restoreGeometry(factoryGeometry_);
    restoreState(factoryLayout_);
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
        return;
    }

    // Switch viewport from software placeholder to Hydra/Storm rendering
    viewportWidget_->setRenderMode(core::RenderMode::LocalHydra);

    // Create the camera toolbar once LocalViewport exists
    if (viewportWidget_->localViewport() && !cameraToolbar_) {
        auto* controller = viewportWidget_->localViewport()->cameraController();
        cameraToolbar_ = new viewport::CameraToolbar(controller, viewportWidget_);
        if (auto* container = centralWidget()) {
            if (auto* layout = qobject_cast<QVBoxLayout*>(container->layout())) {
                layout->insertWidget(0, cameraToolbar_);
            }
        }
    }
}

void MainWindow::onFileClose()
{
    if (simController_->isRunning()) {
        simController_->stop();
    }
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

    // Create a local compute backend and launch
    auto backend = std::make_unique<stubs::LocalComputeBackend>();
    simController_->setComputeBackend(std::move(backend));

    // Default actor mapping for the stub simulation path.
    // The full experiment flow (via ExperimentLoader) populates this from
    // experiment.yaml's scene.actors section. This hardcoded mapping matches
    // StubPhysicsProcess's default "robot" actor to umr_confinement.usda.
    core::PhysicsConfig config;
    config.actorToPrimPath["robot"] = "/World/Actors/UMR";
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

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    connect(&dialog, &SettingsDialog::themeChanged, this, [](const QString& theme) {
        ThemeManager::applyTheme(theme);
    });
    dialog.exec();
}

void MainWindow::onFrameReady(const core::ResultFrame& frame)
{
    // Frame already applied to USD stage by SimulationController::requestNextFrame().
    // Just trigger the viewport repaint and update the status bar.
    viewportWidget_->update();

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
