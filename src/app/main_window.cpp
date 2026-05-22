#include "app/main_window.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QThread>
#include <QVBoxLayout>

#include "app/settings_dialog.h"
#include "app/theme_manager.h"
#include "viewport/viewport_widget.h"
#include "panels/scene_hierarchy_panel.h"
#include "panels/property_panel.h"
#include "panels/timeline_panel.h"
#include "panels/console_widget.h"
#include "panels/mime_console_panel.h"
#include "panels/run_config_panel.h"
#include "panels/skypilot_monitor_panel.h"
#include "panels/project_browser_panel.h"
#include "panels/graph_inspector_panel.h"
#include "panels/script_editor_panel.h"
#include "panels/parameter_panel.h"
#include <QProcess>
#include <nlohmann/json.hpp>
#include "viewport/camera_toolbar.h"
#include "viewport/camera_controller.h"
#include "viewport/local_viewport.h"
#include "stubs/local_compute_backend.h"
#include "experiment/experiment_runner.h"
#include "experiment/experiment_loader.h"
#include "mime_adapter/mime_compute_backend.h"
#include "connection/connection_config.h"

#include <spdlog/spdlog.h>

namespace microbotica::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    MBCA_EXPERIMENTAL_WARN("MainWindow");
    spdlog::info("MainWindow: build with experiment-aware launch path "
                  "(rev: open-experiment-v2)");

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
    if (experimentRunner_ && experimentRunner_->isRunning()) {
        experimentRunner_->stop();
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
    // CameraToolbar with sim controls is created when LocalViewport initializes
    // (in onFileOpen after setRenderMode). Sim controls work without a camera
    // controller, so we create a basic one now for the no-GPU/pre-scene case.
    cameraToolbar_ = new viewport::CameraToolbar(
        nullptr, simController_.get(), this);
    if (auto* container = centralWidget()) {
        if (auto* layout = qobject_cast<QVBoxLayout*>(container->layout())) {
            layout->insertWidget(0, cameraToolbar_);
        }
    }

    // Wire sim controls
    connect(cameraToolbar_, &viewport::CameraToolbar::playRequested,
            this, &MainWindow::onSimulationStart);
    connect(cameraToolbar_, &viewport::CameraToolbar::pauseRequested,
            this, &MainWindow::onSimulationPause);
    connect(cameraToolbar_, &viewport::CameraToolbar::stopRequested,
            this, &MainWindow::onSimulationStop);
    connect(cameraToolbar_, &viewport::CameraToolbar::fullscreenToggled,
            this, [this]() {
        fullscreen_ = !fullscreen_;
        if (fullscreen_) {
            // Hide all dock widgets and menu bar for fullscreen viewport
            for (auto* dock : findChildren<QDockWidget*>()) {
                dock->hide();
            }
            menuBar()->hide();
            statusBar()->hide();
            showFullScreen();
        } else {
            showNormal();
            menuBar()->show();
            statusBar()->show();
            for (auto* dock : findChildren<QDockWidget*>()) {
                dock->show();
            }
        }
    });
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

    auto* openExpAction = fileMenu->addAction(tr("Open &Experiment..."));
    openExpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    openExpAction->setToolTip(
        tr("Open an experiment directory and launch its MIME physics graph."));
    connect(openExpAction, &QAction::triggered,
            this, &MainWindow::onFileOpenExperiment);

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
    // Parameter panel edits → forward to the running MIME backend.
    connect(parameterPanel_, &panels::ParameterPanel::parametersChanged,
            this, [this](const nlohmann::json& params) {
        const bool running =
            simController_ && simController_->isRunning();
        spdlog::info(
            "ParameterPanel → runner: {} keys, sim {} → {}",
            params.size(), running ? "running" : "idle",
            running ? "forwarding" : "DROPPED (sim not running)");
        if (running) {
            simController_->sendParameters(params);
        }
    });

    connect(runConfigPanel_, &panels::RunConfigPanel::experimentDirChanged,
            this, [this](const QString& path) {
        // When the user types / browses for an experiment dir in the
        // RunConfigPanel, bring up the same init flow as the menu
        // action — but do not auto-start the sim, let them press Launch.
        if (!path.isEmpty()) {
            initExperiment(path);
        }
    });

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

    // Wire camera controls to the (now-existing) LocalViewport controller.
    if (viewportWidget_->localViewport() && cameraToolbar_) {
        cameraToolbar_->setCameraController(
            viewportWidget_->localViewport()->cameraController());
    }

    // Detect animation (time-sampled recording) and enter playback mode
    if (sceneMgr_->hasAnimation()) {
        playbackMode_ = true;
        double start = sceneMgr_->startTimeCode();
        double end = sceneMgr_->endTimeCode();
        double fps = sceneMgr_->timeCodesPerSecond();
        int totalFrames = static_cast<int>(end - start) + 1;

        timelinePanel_->enterPlaybackMode(start, end, fps);

        // Connect timeline time code changes to viewport
        connect(timelinePanel_, &panels::TimelinePanel::timeCodeChanged,
                viewportWidget_, &viewport::ViewportWidget::setTimeCode);

        // Show initial frame
        viewportWidget_->setTimeCode(start);
        viewportWidget_->setContinuousRendering(true);

        statusBar()->showMessage(
            tr("Recording loaded: %1 frames at %2 fps")
                .arg(totalFrames).arg(fps, 0, 'f', 1));
    }
}

bool MainWindow::initExperiment(const QString& dirStr)
{
    if (dirStr.isEmpty()) return false;

    const QString manifest = dirStr + "/experiment.yaml";
    if (!QFile::exists(manifest)) {
        spdlog::warn("initExperiment: no experiment.yaml under {}",
                      dirStr.toStdString());
        QMessageBox::warning(this, tr("Not an experiment directory"),
            tr("No experiment.yaml found in:\n%1").arg(dirStr));
        return false;
    }

    // Short-circuit if this is the same experiment already running —
    // avoids re-spawning the subprocess every time the RunConfigPanel
    // re-emits experimentDirChanged.
    if (experimentDir_ == dirStr.toStdString()
        && experimentRunner_ && experimentRunner_->isRunning()) {
        spdlog::info("initExperiment: already running — no-op");
        return true;
    }

    spdlog::info("initExperiment: dir={}", dirStr.toStdString());

    // Stop any existing run before swapping experiments.
    if (simController_->isRunning()) {
        simController_->stop();
    }
    if (experimentRunner_ && experimentRunner_->isRunning()) {
        experimentRunner_->stop();
    }

    // Claim the experiment directory up front so that any downstream
    // failure still leaves the app wired to the MIME backend on the
    // user's next Launch press.
    experimentDir_ = dirStr.toStdString();

    // Load the experiment's USD scene so the viewport has a stage.
    const QStringList candidates = {
        dirStr + "/scene/world.usda",
        dirStr + "/scene/world.usd",
        dirStr + "/scene/world.usdc",
    };
    QString sceneUsd;
    for (const QString& c : candidates) {
        if (QFile::exists(c)) { sceneUsd = c; break; }
    }
    if (!sceneUsd.isEmpty() && !sceneMgr_->isLoaded()) {
        spdlog::info("initExperiment: loading scene {}",
                      sceneUsd.toStdString());
        if (sceneMgr_->loadScene(sceneUsd.toStdString())) {
            viewportWidget_->setRenderMode(core::RenderMode::LocalHydra);
            if (viewportWidget_->localViewport() && cameraToolbar_) {
                cameraToolbar_->setCameraController(
                    viewportWidget_->localViewport()->cameraController());
            }
        } else {
            spdlog::warn("initExperiment: scene load returned false");
            QMessageBox::warning(this, tr("Scene load failed"),
                tr("Experiment scene file failed to load:\n%1\n"
                   "Open it manually via File → Open USD Scene.").arg(sceneUsd));
        }
    } else if (sceneUsd.isEmpty()) {
        spdlog::warn("initExperiment: no scene/world.usd[a|c] under {}",
                      dirStr.toStdString());
    }

    // Note: the MIME runner subprocess is NOT spawned here. It is
    // spawned (and re-spawned after Stop) by onSimulationStart so the
    // user controls when physics actually starts. initExperiment only
    // claims the directory and primes the UI panels.

    // ── Project Browser: root the file tree at the experiment dir ──
    if (projectBrowser_) {
        projectBrowser_->setExperimentDir(dirStr.toStdString());
        spdlog::info("ProjectBrowser: rooted at {}", dirStr.toStdString());
    }

    // ── Parameter Panel: dump physics/params.py to JSON via python3 ──
    const QString paramsPy = dirStr + "/physics/params.py";
    if (parameterPanel_ && QFile::exists(paramsPy)) {
        QProcess p;
        QStringList args;
        args << "-c"
             << QString(
                "import json,runpy;"
                "ns=runpy.run_path('%1');"
                "out={k:v for k,v in ns.items() "
                "    if not k.startswith('_') "
                "    and isinstance(v,(int,float,bool,str))};"
                "print(json.dumps(out))").arg(paramsPy);
        p.start("python3", args);
        if (p.waitForFinished(5000) && p.exitCode() == 0) {
            try {
                auto j = nlohmann::json::parse(
                    p.readAllStandardOutput().toStdString());
                parameterPanel_->loadParameters(j);
                spdlog::info("ParameterPanel: loaded {} params",
                              j.size());
            } catch (const std::exception& e) {
                spdlog::warn("ParameterPanel: params.py parse failed: {}",
                              e.what());
            }
        } else {
            spdlog::warn(
                "ParameterPanel: python3 dump of params.py failed "
                "(exit={}, stderr={})", p.exitCode(),
                p.readAllStandardError().toStdString());
        }
    } else if (parameterPanel_) {
        spdlog::warn("ParameterPanel: no physics/params.py at {}",
                      paramsPy.toStdString());
    }

    // ── Graph Inspector: load graph.json. If the runner has never been
    // launched in this dir, graph.json may not exist yet. Attempt
    // again after the first launch. ──
    const QString graphJson = dirStr + "/graph.json";
    if (graphInspector_ && QFile::exists(graphJson)) {
        try {
            QFile f(graphJson);
            if (f.open(QIODevice::ReadOnly)) {
                auto j = nlohmann::json::parse(
                    f.readAll().toStdString());
                graphInspector_->loadFromJson(j);
                spdlog::info("GraphInspector: loaded graph.json ({} nodes)",
                              j.value("nodes", nlohmann::json::object()).size());
            }
        } catch (const std::exception& e) {
            spdlog::warn("GraphInspector: graph.json parse failed: {}",
                          e.what());
        }
    } else if (graphInspector_) {
        spdlog::info(
            "GraphInspector: graph.json not yet present at {} "
            "(will load after first Launch)", graphJson.toStdString());
    }

    statusBar()->showMessage(
        tr("Experiment ready: %1 — press Launch to begin")
            .arg(QFileInfo(dirStr).fileName()));
    if (runConfigPanel_) {
        runConfigPanel_->setConnectionStatus(tr("Loaded — not running"));
    }
    return true;
}

void MainWindow::onFileOpenExperiment()
{
    const QString dirStr = QFileDialog::getExistingDirectory(
        this,
        tr("Open Experiment Directory"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dirStr.isEmpty()) return;

    if (!initExperiment(dirStr)) return;

    // Mirror the directory into RunConfigPanel (guarded so the
    // re-entrant experimentDirChanged → initExperiment chain doesn't
    // double-fire).
    if (runConfigPanel_) {
        QSignalBlocker blocker(runConfigPanel_);
        Q_EMIT runConfigPanel_->experimentDirChanged(dirStr);
    }
}

void MainWindow::onFileClose()
{
    if (simController_->isRunning()) {
        paused_ = false;
        simController_->setPaused(false);
        simController_->stop();
    }
    if (playbackMode_) {
        playbackMode_ = false;
        timelinePanel_->enterSimulationMode();
        viewportWidget_->resetTimeCode();
        viewportWidget_->setContinuousRendering(false);
        disconnect(timelinePanel_, &panels::TimelinePanel::timeCodeChanged,
                   viewportWidget_, &viewport::ViewportWidget::setTimeCode);
    }
    primSelection_->clear();
    sceneMgr_->closeScene();
    viewportWidget_->setRenderMode(core::RenderMode::Software);
#ifdef MICROBOTICA_HAS_USD
    viewportWidget_->setStage(nullptr);
#endif
    statusBar()->showMessage(tr("Scene closed"));
}

void MainWindow::onSimulationStart()
{
    spdlog::info("onSimulationStart: ENTRY  experimentDir='{}' "
                  "running={} paused={} playback={}",
                  experimentDir_, simController_->isRunning(),
                  paused_, playbackMode_);
    // Playback mode: play/resume the animation
    if (playbackMode_) {
        timelinePanel_->playbackPlay();
        viewportWidget_->setContinuousRendering(true);
        statusBar()->showMessage(tr("Playback started"));
        if (cameraToolbar_) cameraToolbar_->onSimStarted();
        return;
    }

    if (!sceneMgr_->isLoaded()) {
        // If an experiment has been opened but its scene wasn't auto-loaded
        // (e.g. the user picked the manifest dir before Launch), try the
        // conventional scene/world.usda path now.
        bool loaded = false;
        if (!experimentDir_.empty()) {
            const QString sceneUsd =
                QString::fromStdString(experimentDir_) + "/scene/world.usda";
            if (QFile::exists(sceneUsd)
                && sceneMgr_->loadScene(sceneUsd.toStdString())) {
                viewportWidget_->setRenderMode(core::RenderMode::LocalHydra);
                if (viewportWidget_->localViewport() && cameraToolbar_) {
                    cameraToolbar_->setCameraController(
                        viewportWidget_->localViewport()->cameraController());
                }
                loaded = true;
            }
        }
        if (!loaded) {
            QMessageBox::information(this, tr("No Scene"),
                                     tr("Please open a USD scene before starting simulation."));
            return;
        }
    }

    // Resume from pause
    if (paused_ && simController_->isRunning()) {
        paused_ = false;
        simController_->setPaused(false);
        timelinePanel_->resumePolling();
        viewportWidget_->setContinuousRendering(true);
        statusBar()->showMessage(tr("Simulation resumed"));
        if (cameraToolbar_) cameraToolbar_->onSimStarted();
        return;
    }

    if (simController_->isRunning()) return;

    // If an experiment directory is remembered but the previous Stop
    // tore down the MIME runner, re-spawn it now so the user can press
    // Launch again without re-opening the experiment from the menu.
    // Try up to 3 times with a 1 s back-off — covers TIME_WAIT or
    // overlapping prior shutdowns.
    if (!experimentDir_.empty()
        && (!experimentRunner_ || !experimentRunner_->isRunning())) {
        spdlog::info("onSimulationStart: re-spawning MIME runner for {}",
                      experimentDir_);
        if (!experimentRunner_) {
            experimentRunner_ =
                std::make_unique<experiment::ExperimentRunner>(this);
            // Wire stdout/stderr → MimeConsolePanel so the user sees the
            // Python tracebacks and runner progress in the bottom dock.
            if (mimeConsole_) {
                connect(experimentRunner_.get(),
                        &experiment::ExperimentRunner::stdoutChunk,
                        mimeConsole_,
                        &panels::MimeConsolePanel::appendStdout);
                connect(experimentRunner_.get(),
                        &experiment::ExperimentRunner::stderrChunk,
                        mimeConsole_,
                        &panels::MimeConsolePanel::appendStderr);
            }
        }

        // Stream format requested in the Run Configuration panel (fit-up
        // §8): "" Auto, "json", or "binary" → MIME_STREAM_FORMAT.
        const std::string streamFormat =
            runConfigPanel_ ? runConfigPanel_->streamFormat() : std::string();

        bool spawned = false;
        for (int attempt = 1; attempt <= 3; ++attempt) {
            if (experimentRunner_->start(experimentDir_, streamFormat)) {
                spawned = true;
                break;
            }
            spdlog::warn("onSimulationStart: re-spawn attempt {} failed; "
                          "retrying...", attempt);
            QThread::msleep(1000);
        }
        if (!spawned) {
            spdlog::error("onSimulationStart: re-spawn failed after 3 attempts");
            QMessageBox::critical(this, tr("Experiment failed to start"),
                tr("Could not re-launch the MIME runner subprocess after 3 "
                   "attempts. Falling back to stub backend."));
        }
    }

    // The MIME runner spends ~2 s building the graph + JIT-compiling
    // before it binds its ZMQ ports. Probe the REP port (5555) until it
    // accepts so the upcoming MimePhysicsProcess::launch sendCommand()
    // doesn't hit a 5 s receive timeout against an unbound port.
    if (!experimentDir_.empty()
        && experimentRunner_ && experimentRunner_->isRunning()) {
        statusBar()->showMessage(tr("Waiting for MIME runner..."));
        QApplication::processEvents();
        if (!experimentRunner_->waitUntilReady(30000, 5555)) {
            spdlog::error("onSimulationStart: MIME runner not ready");
            QMessageBox::critical(this, tr("Experiment startup timeout"),
                tr("The MIME runner subprocess did not finish initialising "
                   "within 30 s. Check the console for a Python traceback."));
        } else {
            // graph.json is now written by the runner. Refresh the
            // GraphInspector if it didn't pick it up during init.
            const QString graphJson =
                QString::fromStdString(experimentDir_) + "/graph.json";
            if (graphInspector_ && QFile::exists(graphJson)) {
                QFile f(graphJson);
                if (f.open(QIODevice::ReadOnly)) {
                    try {
                        auto j = nlohmann::json::parse(
                            f.readAll().toStdString());
                        graphInspector_->loadFromJson(j);
                        spdlog::info(
                            "GraphInspector: refreshed after Launch ({} nodes)",
                            j.value("nodes",
                                     nlohmann::json::object()).size());
                    } catch (...) { /* already-logged in init path */ }
                }
            }
        }
    }

    core::PhysicsConfig config;

    const bool have_experiment = !experimentDir_.empty()
        && experimentRunner_ && experimentRunner_->isRunning();
    spdlog::info("onSimulationStart: experimentDir='{}' runner={}  → {}",
                  experimentDir_,
                  (experimentRunner_ ? (experimentRunner_->isRunning()
                                         ? "running" : "stopped")
                                     : "null"),
                  have_experiment ? "MimeComputeBackend"
                                  : "LocalComputeBackend (stub)");

    if (have_experiment) {
        // Connect to the already-running MIME subprocess via ZMQ.
        connection::ConnectionConfig cc;
        cc.mode = connection::ConnectionMode::Local;
        auto backend = std::make_unique<mime::MimeComputeBackend>(cc);
        simController_->setComputeBackend(std::move(backend));

        // Actor map: parse experiment.yaml/scene.actors via
        // ExperimentLoader → ExperimentConfig::buildPhysicsConfig().
        // ResultsApplicator looks up every actor name from the
        // ResultFrame in this map; missing entries log
        // "No prim path mapping for actor X — skipping" and the
        // corresponding USD prim never moves.
        experiment::ExperimentLoader loader;
        auto exp_cfg = loader.load(experimentDir_);
        if (exp_cfg) {
            config = exp_cfg->buildPhysicsConfig();
            spdlog::info(
                "MainWindow: registered {} actor(s) from experiment.yaml",
                config.actorToPrimPath.size());
        } else {
            spdlog::warn(
                "MainWindow: ExperimentLoader::load failed for '{}' "
                "— falling back to legacy hardcoded body actor",
                experimentDir_);
            config.actorToPrimPath["body"] = "/World/Actors/UMR";
        }
    } else {
        // Legacy stub-backend path (no MIME runner) — kept so the
        // viewport demo still works without an experiment.
        auto backend = std::make_unique<stubs::LocalComputeBackend>();
        simController_->setComputeBackend(std::move(backend));
        config.actorToPrimPath["robot"] = "/World/Actors/UMR";
    }

    simController_->launchPhysics(config);

    paused_ = false;
    statusBar()->showMessage(tr("Simulation started"));
}

void MainWindow::onSimulationPause()
{
    // Playback mode: pause animation
    if (playbackMode_) {
        timelinePanel_->playbackPause();
        viewportWidget_->setContinuousRendering(false);
        statusBar()->showMessage(tr("Playback paused"));
        if (cameraToolbar_) cameraToolbar_->onSimPaused();
        return;
    }

    if (!simController_->isRunning() || paused_) return;

    paused_ = true;
    simController_->setPaused(true);
    timelinePanel_->pausePolling();
    viewportWidget_->setContinuousRendering(false);
    statusBar()->showMessage(tr("Simulation paused"));
    if (cameraToolbar_) cameraToolbar_->onSimPaused();
}

void MainWindow::onSimulationStop()
{
    // Playback mode: stop and seek to frame 0
    if (playbackMode_) {
        timelinePanel_->playbackStop();
        viewportWidget_->setTimeCode(sceneMgr_->startTimeCode());
        viewportWidget_->setContinuousRendering(false);
        statusBar()->showMessage(tr("Playback stopped"));
        if (cameraToolbar_) cameraToolbar_->onSimStopped();
        return;
    }

    paused_ = false;
    simController_->setPaused(false);
    simController_->stop();

    // If we're driving a MIME-runner experiment, terminate the
    // subprocess too — otherwise it keeps stepping in the background
    // and the next Launch finds the ZMQ ports already bound.
    if (experimentRunner_ && experimentRunner_->isRunning()) {
        spdlog::info("onSimulationStop: tearing down ExperimentRunner");
        experimentRunner_->stop();
    }

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
