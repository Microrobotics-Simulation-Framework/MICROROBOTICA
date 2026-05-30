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
#include "viewport/notice_overlay.h"
#include <QTimer>
#include <chrono>
#include "stubs/local_compute_backend.h"
#include "experiment/experiment_runner.h"
#include "experiment/experiment_loader.h"
#include "experiment/cloud_job_launcher.h"
#include "mime_adapter/mime_compute_backend.h"
#include "mime_adapter/mime_physics_process.h"
#include "connection/connection_config.h"
#include "connection/connection_manager.h"
#include "connection/ssh_tunnel.h"

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

    // Top-level transparent-input notice overlay that follows the viewport
    // (must be top-level — LocalViewport is a native window that hides
    // sibling Qt widgets). Parented to `this` for lifetime management.
    noticeOverlay_ = new viewport::NoticeOverlay(viewportWidget_);

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

    applyNoticeSettings();
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

    // scripts_dir for sky_resolve.py — resolve next to the application
    // binary first (the installed/AppImage layout), fall back to the
    // source tree (typical when running from `build/debug/...`). The
    // previous "." placeholder meant the panel's QProcess looked for
    // `./sky_resolve.py` in whatever cwd MICROROBOTICA was launched
    // from, which was almost never the right place — the panel never
    // populated.  Mirror what pollClusterReady does at probe time.
    QString scriptsDirQ = QCoreApplication::applicationDirPath() + "/scripts";
    if (!QFile::exists(scriptsDirQ + "/sky_resolve.py")) {
        scriptsDirQ = QStringLiteral(
            "/home/nick/MSF/msf/MICROROBOTICA/scripts");
    }
    skypilotPanel_ = new panels::SkyPilotMonitorPanel(
        scriptsDirQ.toStdString(), this);
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

        // Show initial frame. No continuous rendering for recordings — the
        // viewport repaints on demand (per played-back frame via setTimeCode,
        // and on camera moves); a paused recording stays off the GPU.
        viewportWidget_->setTimeCode(start);

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

    // Load the experiment's USD scene so the viewport has a stage. If a
    // previous experiment's scene is loaded, close it first — otherwise
    // switching experiments would leave the old world.usda on screen.
    const QStringList candidates = {
        dirStr + "/scene/world.usda",
        dirStr + "/scene/world.usd",
        dirStr + "/scene/world.usdc",
    };
    QString sceneUsd;
    for (const QString& c : candidates) {
        if (QFile::exists(c)) { sceneUsd = c; break; }
    }
    if (!sceneUsd.isEmpty()) {
        if (sceneMgr_->isLoaded()) {
            spdlog::info("initExperiment: closing previous scene before "
                          "loading the new one");
            sceneMgr_->closeScene();
        }
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
    } else {
        spdlog::warn("initExperiment: no scene/world.usd[a|c] under {}",
                      dirStr.toStdString());
    }

    // Mirror the path into the Run Configuration panel so File → Open
    // Experiment shows up in the folder selector. setExperimentDir does
    // not emit experimentDirChanged, so this won't re-enter initExperiment.
    if (runConfigPanel_) {
        runConfigPanel_->setExperimentDir(dirStr.toStdString());
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

    // initExperiment mirrors the path into runConfigPanel_ via
    // setExperimentDir, so nothing else is needed here.
    initExperiment(dirStr);
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
        // Playback drives the viewport on demand: the playback timer emits a
        // new timecode per frame, each of which repaints. No continuous loop.
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

    // Guard against re-entry while a Cloud-mode launch is mid-flight.
    // simController_->isRunning() stays false from the moment Launch is
    // clicked until finishSimulationStart() finally runs — a window of
    // several minutes that spans `sky launch` dispatch + pollClusterReady
    // + SSH tunnel open + waitForRunnerReady.  The previous early-returns
    // only catch the running/paused states; without this guard, the
    // toolbar Play button (or another Launch click) during the in-flight
    // window re-entered startCloudLaunch and tried to spawn a *second*
    // SkyPilot cluster, exactly what the user just hit.
    //
    // tearDownCloud clears cloudClusterName_ so a subsequent Launch
    // after Stop works correctly; that's the only signal we need.
    if (!cloudClusterName_.isEmpty() && cloudJobLauncher_) {
        spdlog::info("onSimulationStart: cloud launch already in flight "
                      "for cluster '{}'; ignoring duplicate request",
                      cloudClusterName_.toStdString());
        statusBar()->showMessage(
            tr("Cloud launch already in flight for '%1' — wait for it "
               "to complete, or click Stop to cancel.")
                .arg(cloudClusterName_));
        return;
    }

    // Route on connection mode. Cloud goes through the async SkyPilot
    // orchestration (submit job → wait UP → open SSH tunnel → handshake);
    // Local spawns a python subprocess; Manual skips spawning and connects
    // to whatever endpoint is configured.
    const connection::ConnectionMode mode = runConfigPanel_
        ? runConfigPanel_->selectedMode()
        : connection::ConnectionMode::Local;
    if (mode == connection::ConnectionMode::Cloud) {
        startCloudLaunch();
        return;
    }

    // Manual mode: skip the local subprocess spawn entirely.
    if (mode == connection::ConnectionMode::Manual) {
        finishSimulationStart();
        return;
    }

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

        // Stream format + publish rate requested in the Run Configuration
        // panel (fit-up §8): pass through as MIME_STREAM_FORMAT and
        // MIME_PUBLISH_FPS on the spawned runner's environment.
        const std::string streamFormat =
            runConfigPanel_ ? runConfigPanel_->streamFormat() : std::string();
        const int publishFps =
            runConfigPanel_ ? runConfigPanel_->publishFps() : 0;

        bool spawned = false;
        for (int attempt = 1; attempt <= 3; ++attempt) {
            if (experimentRunner_->start(experimentDir_, streamFormat,
                                          publishFps)) {
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

    // The MIME runner spends a few seconds building the graph + JIT-compiling
    // before its ZMQ ports accept. Defer the rest of the launch sequence
    // until the REP port is up, keeping the Qt event loop running so the
    // viewport stays interactive and the notice overlay can render.
    if (!experimentDir_.empty()
        && experimentRunner_ && experimentRunner_->isRunning()) {
        startWaitingForRunner();
        return;
    }

    // No experiment dir → stub-backend path: finish synchronously.
    finishSimulationStart();
}

void MainWindow::startWaitingForRunner()
{
    statusBar()->showMessage(tr("Waiting for MIME runner..."));
    if (runConfigPanel_) {
        runConfigPanel_->setLaunchEnabled(false);
        runConfigPanel_->setConnectionStatus(tr("Waiting for runner..."));
    }
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("waiting-mime"),
            viewport::NoticeOverlay::Severity::Info,
            tr("Starting MIME runner"),
            tr("Compiling the physics graph and binding ZMQ sockets — "
               "this usually takes a few seconds. "
               "You can keep navigating the viewport while it finishes."));
    }

    auto* timer = new QTimer(this);
    // 800ms (was 400ms) — drops UI-thread occupancy during the ~12s
    // local-mode wait from 75% to ~25%, eliminating the viewport
    // stutter that was visible while the runner did first-step JIT.
    // Detection lag worst-case +800ms is invisible against the wait.
    timer->setInterval(800);
    const auto start = std::chrono::steady_clock::now();
    connect(timer, &QTimer::timeout, this, [this, timer, start]() {
        if (!experimentRunner_ || !experimentRunner_->isRunning()) {
            timer->stop();
            timer->deleteLater();
            spdlog::error("startWaitingForRunner: runner stopped before becoming ready");
            finishWaitingForRunner(false);
            QMessageBox::critical(this, tr("Experiment failed"),
                tr("The MIME runner stopped before becoming ready. "
                   "Check the console for a Python traceback."));
            return;
        }
        // Application-level readiness — TCP accept on 5555 fires the
        // moment ZMQ binds, but the runner's main loop is still doing
        // first-step JIT for several seconds after that and any command
        // queued during that window gets a reply only after JIT finishes
        // (by which time MimePhysicsProcess::sendCommand's REQ socket
        // has timed out with LINGER=0, so the reply is dropped on the
        // floor). Probe via a fresh stream_info round-trip — the runner
        // only answers when its loop has actually iterated past JIT.
        if (mime::MimePhysicsProcess::probeRunner("tcp://localhost:5555",
                                                    300)) {
            timer->stop();
            timer->deleteLater();
            finishWaitingForRunner(true);
            return;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        // 60 s covers: graph build + JIT (slower under container CPU
        // caps), ZMQ bind, and the first-step JIT inside the main loop.
        // The standalone runner on host hits all three in ~6 s; the
        // cloud-dev container with --cpus=6 doubles that.
        if (elapsed > std::chrono::seconds(60)) {
            timer->stop();
            timer->deleteLater();
            spdlog::error("startWaitingForRunner: MIME runner not ready in 60s");
            finishWaitingForRunner(false);
            QMessageBox::critical(this, tr("Experiment startup timeout"),
                tr("The MIME runner subprocess did not become responsive "
                   "within 60 s (stream_info round-trip). "
                   "Check the console for a Python traceback."));
        }
    });
    timer->start();
}

void MainWindow::finishWaitingForRunner(bool ok)
{
    if (noticeOverlay_) {
        noticeOverlay_->removeNotice(QStringLiteral("waiting-mime"));
    }
    if (!ok) {
        statusBar()->showMessage(tr("Experiment failed to start"));
        if (runConfigPanel_) {
            runConfigPanel_->setLaunchEnabled(true);
            runConfigPanel_->setConnectionStatus(tr("Idle"));
        }
        return;
    }
    // graph.json is now written by the runner. Refresh the GraphInspector
    // if it didn't pick it up during init.
    if (!experimentDir_.empty()) {
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
    finishSimulationStart();
}

void MainWindow::finishSimulationStart()
{
    core::PhysicsConfig config;

    const connection::ConnectionMode mode = runConfigPanel_
        ? runConfigPanel_->selectedMode()
        : connection::ConnectionMode::Local;
    const bool have_local_runner = !experimentDir_.empty()
        && experimentRunner_ && experimentRunner_->isRunning();
    const bool have_cloud = (mode == connection::ConnectionMode::Cloud)
        && !cloudClusterName_.isEmpty()
        && sshTunnel_ && sshTunnel_->isOpen();
    const bool have_manual = (mode == connection::ConnectionMode::Manual);
    const bool have_mime = have_local_runner || have_cloud || have_manual;

    spdlog::info("finishSimulationStart: mode={} experimentDir='{}' "
                  "local_runner={} cloud_tunnel={} → {}",
                  static_cast<int>(mode), experimentDir_,
                  have_local_runner, have_cloud,
                  have_mime ? "MimeComputeBackend"
                            : "LocalComputeBackend (stub)");

    if (have_mime) {
        // Connect via ZMQ — the endpoint is resolved by ConnectionManager
        // based on `cc.mode`. For Cloud the SSH tunnel is already open,
        // so the resolved endpoint is localhost:5556 (tunnelled).
        connection::ConnectionConfig cc;
        cc.mode = mode;
        if (mode == connection::ConnectionMode::Cloud) {
            cc.skypilot_cluster = cloudClusterName_.toStdString();
        }
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

// ── Cloud orchestration ─────────────────────────────────────────────────────
//
// Submit `sky launch -c <cluster> -y -d` → poll `sky_resolve.py` until the
// cluster reaches UP → open an SshTunnel to the resolved IP forwarding
// 5555/5556 → probe localhost:5555 until accepting → hand off to
// finishSimulationStart() which behaves exactly as it does for Local.

void MainWindow::startCloudLaunch()
{
    cloudClusterName_ = runConfigPanel_
        ? QString::fromStdString(runConfigPanel_->clusterName())
        : QString();
    if (cloudClusterName_.isEmpty()) {
        QMessageBox::critical(this, tr("Missing cluster name"),
            tr("Cloud mode needs a SkyPilot cluster name in the Run "
               "Configuration panel."));
        return;
    }
    cloudStopOnDisconnect_ = runConfigPanel_
        && runConfigPanel_->stopClusterOnDisconnect();

    // Job config: panel override first, then experiment.yaml's cloud.job_config.
    std::string jobConfig = runConfigPanel_ ? runConfigPanel_->jobConfig()
                                             : std::string();
    if (jobConfig.empty() && !experimentDir_.empty()) {
        experiment::ExperimentLoader loader;
        auto cfg = loader.load(experimentDir_);
        if (cfg && cfg->cloud) {
            jobConfig = cfg->cloud->job_config;
        }
    }
    if (jobConfig.empty()) {
        QMessageBox::critical(this, tr("Missing job config"),
            tr("Cloud mode needs a SkyPilot job YAML — set Job config in "
               "the panel or add `cloud.job_config:` to experiment.yaml."));
        return;
    }

    if (!cloudJobLauncher_) {
        cloudJobLauncher_ = std::make_unique<experiment::CloudJobLauncher>(this);
        connect(cloudJobLauncher_.get(),
                &experiment::CloudJobLauncher::launchSucceeded,
                this, &MainWindow::onCloudJobLaunched);
        connect(cloudJobLauncher_.get(),
                &experiment::CloudJobLauncher::launchFailed,
                this, [this](const QString& err) {
            onCloudFailed(tr("submitting SkyPilot job"), err);
        });
    }

    if (runConfigPanel_) {
        runConfigPanel_->setLaunchEnabled(false);
        runConfigPanel_->setConnectionStatus(tr("Submitting cloud job..."));
    }
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("cloud-launch"),
            viewport::NoticeOverlay::Severity::Info,
            tr("Submitting SkyPilot job"),
            tr("Cluster: %1 — `sky launch -c %1 -y -d` is running. The "
               "viewport stays interactive throughout.")
                .arg(cloudClusterName_));
    }

    if (!cloudJobLauncher_->launch(experimentDir_, jobConfig,
                                     cloudClusterName_.toStdString())) {
        onCloudFailed(tr("submitting SkyPilot job"),
                       tr("launch() returned false — see the console"));
    }
}

void MainWindow::onCloudJobLaunched(const QString& stdout_output)
{
    spdlog::info("MainWindow: sky launch returned for cluster '{}'",
                  cloudClusterName_.toStdString());

    // Parse the job ID from sky's stdout ("Job submitted, ID: N").
    // If absent (older sky versions, terse output), fall back to
    // `sky logs -f <cluster>` which follows the cluster's latest job.
    int jobId = -1;
    for (const QString& line : stdout_output.split('\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QStringLiteral("Job submitted, ID:"))) {
            bool ok = false;
            const int id = trimmed.section(':', 1, 1).trimmed().toInt(&ok);
            if (ok) {
                jobId = id;
                break;
            }
        }
    }

    // Start the persistent `sky logs -f` follower piped into the
    // MimeConsolePanel.  In Cloud mode the runner is in the SkyPilot pod,
    // so its stdout/stderr live on sky's side — the Local-mode QProcess
    // hookup in onSimulationStart never fires.  This is the equivalent
    // for the remote runner.
    if (cloudLogFollower_) {
        cloudLogFollower_->kill();
        cloudLogFollower_->waitForFinished(1000);
        cloudLogFollower_->deleteLater();
        cloudLogFollower_ = nullptr;
    }
    cloudLogFollower_ = new QProcess(this);
    QStringList logArgs;
    // `--follow` (not `-f`) — sky 0.12.x only accepts the long form.
    logArgs << QStringLiteral("logs") << QStringLiteral("--follow")
            << cloudClusterName_;
    if (jobId > 0) logArgs << QString::number(jobId);
    if (mimeConsole_) {
        // Tag the source so the user can tell remote logs apart from
        // Local-mode runner output if both have appeared in the panel.
        const QByteArray header = (QStringLiteral("[sky logs --follow %1%2]\n")
            .arg(cloudClusterName_)
            .arg(jobId > 0 ? QStringLiteral(" %1").arg(jobId)
                            : QString())).toUtf8();
        mimeConsole_->appendStdout(header);
        connect(cloudLogFollower_, &QProcess::readyReadStandardOutput,
                this, [this]() {
            if (cloudLogFollower_ && mimeConsole_) {
                mimeConsole_->appendStdout(
                    cloudLogFollower_->readAllStandardOutput());
            }
        });
        connect(cloudLogFollower_, &QProcess::readyReadStandardError,
                this, [this]() {
            if (cloudLogFollower_ && mimeConsole_) {
                mimeConsole_->appendStderr(
                    cloudLogFollower_->readAllStandardError());
            }
        });
    }
    cloudLogFollower_->start(QStringLiteral("sky"), logArgs);
    if (!cloudLogFollower_->waitForStarted(2000)) {
        spdlog::warn("MainWindow: `sky logs --follow {}` failed to start "
                      "({}) — remote runner output won't reach the "
                      "MimeConsolePanel for this session",
                      cloudClusterName_.toStdString(),
                      cloudLogFollower_->errorString().toStdString());
    } else {
        spdlog::info("MainWindow: streaming `sky logs --follow {} {}` into "
                      "MimeConsolePanel",
                      cloudClusterName_.toStdString(),
                      jobId > 0 ? std::to_string(jobId) : std::string("(latest)"));
    }

    // The SkyPilotMonitorPanel polls every 30s by default. Kick a
    // refresh now so the new cluster shows up immediately instead of
    // up to 30s late.
    if (skypilotPanel_) skypilotPanel_->refresh();

    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("cloud-launch"),
            viewport::NoticeOverlay::Severity::Info,
            tr("Waiting for cluster"),
            tr("`sky launch` returned; polling `sky_resolve.py` until "
               "cluster '%1' reports status: UP. This can take several "
               "minutes the first time. The MIME runner's stdout is "
               "streaming into the MIME Console panel below.")
                .arg(cloudClusterName_));
    }
    pollClusterReady();
}

void MainWindow::pollClusterReady()
{
    auto* timer = new QTimer(this);
    timer->setInterval(5000);
    const auto start = std::chrono::steady_clock::now();
    connect(timer, &QTimer::timeout, this, [this, timer, start]() {
        connection::ConnectionManager mgr;
        // Look for sky_resolve.py next to the binary first; fall back to
        // the source tree (typical when running from `build/debug/...`).
        QString scriptsDir = QCoreApplication::applicationDirPath() + "/scripts";
        if (!QFile::exists(scriptsDir + "/sky_resolve.py")) {
            scriptsDir = QStringLiteral(
                "/home/nick/MSF/msf/MICROROBOTICA/scripts");
        }
        auto result = mgr.resolveCluster(
            cloudClusterName_.toStdString(), scriptsDir.toStdString());
        if (result.has_value()
            && result->value("status", "") == "UP") {
            timer->stop();
            timer->deleteLater();
            const std::string ip = result->value("ip", "");
            const int port = result->value("ssh_port", 22);
            openCloudTunnel(QString::fromStdString(ip), port);
            return;
        }
        if (std::chrono::steady_clock::now() - start
            > std::chrono::minutes(10)) {
            timer->stop();
            timer->deleteLater();
            onCloudFailed(tr("waiting for cluster"),
                tr("cluster did not reach UP status within 10 minutes"));
        }
    });
    timer->start();
}

void MainWindow::openCloudTunnel(const QString& ip, int ssh_port)
{
    spdlog::info("MainWindow: opening SSH tunnel to {}:{} for cluster '{}'",
                  ip.toStdString(), ssh_port, cloudClusterName_.toStdString());
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("cloud-launch"),
            viewport::NoticeOverlay::Severity::Info,
            tr("Opening SSH tunnel"),
            tr("Forwarding ZMQ 5555/5556 from %1 to localhost via SSH...")
                .arg(ip));
    }

    if (!sshTunnel_) {
        sshTunnel_ = std::make_unique<connection::SshTunnel>(this);
        connect(sshTunnel_.get(), &connection::SshTunnel::tunnelDied,
                this, [this](int code, const QString& err) {
            if (noticeOverlay_) {
                noticeOverlay_->addNotice(
                    QStringLiteral("cloud-launch"),
                    viewport::NoticeOverlay::Severity::Error,
                    tr("SSH tunnel died"),
                    tr("ssh exited %1: %2 — the MIME backend will time out "
                       "shortly.").arg(code).arg(err));
            }
        });
    }

    // SkyPilot SSH node pool deploys k3s; the task's `ports:` are exposed
    // via a LoadBalancer service whose ingress IP is the node's external
    // interface. That means the forwarding target must be the same host
    // (not the SSH-server's localhost — nothing listens there). The user
    // (`nick` here) matches the ssh_node_pools.yaml entry.
    const std::vector<int> ports = {5555, 5556};
    if (!sshTunnel_->open(ip.toStdString(), ssh_port, ports, ports,
                           "nick", ip.toStdString())) {
        onCloudFailed(tr("opening SSH tunnel"),
                       tr("SshTunnel::open returned false"));
        return;
    }

    auto* tcp_timer = new QTimer(this);
    // 500ms (was 200ms) — the SSH tunnel + TCP accept on localhost:5555
    // typically completes in ~1-2s after `ssh -L` starts, so 200ms was
    // burning UI thread time on probes that would equally well land on
    // the next tick.  500ms = ~16% UI occupancy on the 80ms probe vs
    // 40% before.
    tcp_timer->setInterval(500);
    const auto tcp_start = std::chrono::steady_clock::now();
    connect(tcp_timer, &QTimer::timeout, this, [this, tcp_timer, tcp_start]() {
        if (sshTunnel_ && sshTunnel_->isOpen()
            && experiment::ExperimentRunner::probePort(5555, 80)) {
            tcp_timer->stop();
            tcp_timer->deleteLater();
            waitForRunnerReady();  // ZMQ-level probe (long window).
            return;
        }
        if (std::chrono::steady_clock::now() - tcp_start
            > std::chrono::seconds(60)) {
            tcp_timer->stop();
            tcp_timer->deleteLater();
            onCloudFailed(tr("waiting for tunnel"),
                tr("ssh -L opened but localhost:5555 never accepted (60s) — "
                   "is the MIME runner actually listening on the cluster?"));
        }
    });
    tcp_timer->start();
}

// SSH `-L` accepts on the local socket whether the remote is bound or not,
// so the prior probePort(5555) gate only confirms the tunnel itself is up.
// To know the MIME runner has actually bound its REP socket — past the
// SkyPilot pod's setup (apt + JAX install) and JIT compile — we need an
// application-layer handshake. probeRunner() opens a fresh REQ socket on
// 5556 and sends `stream_info`; only a reply counts. The cold-start window
// for a fresh pod can be 10+ minutes; we cap at 20 to be safe.
void MainWindow::waitForRunnerReady()
{
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("cloud-launch"),
            viewport::NoticeOverlay::Severity::Info,
            tr("Waiting for MIME runner"),
            tr("Tunnel up. The pod is in setup (apt + JAX install + JIT). "
               "This is normal — first launch can take 5–10 minutes."));
    }

    auto* zmq_timer = new QTimer(this);
    // Probe every 5s with a 1500ms blocking timeout per probe.  Previously
    // 2s / 1s — 50% UI-thread occupancy while waiting, which visibly
    // hitched the OpenGL viewport for 5-10 minutes during the pod's
    // pip-install-JAX phase.  5s / 1500ms drops that to ~30%.  300ms was
    // too tight for SSH-tunnelled ZMQ round-trips and caused the probe
    // to time out before the runner could reply, so waitForRunnerReady
    // never completed and finishSimulationStart never ran — exactly
    // what the user just hit ("no rendering even though sky logs
    // showed steps publishing").  Detecting readiness ~5s later than
    // optimal is invisible against the multi-minute pod-setup wait.
    zmq_timer->setInterval(5000);
    const auto zmq_start = std::chrono::steady_clock::now();
    connect(zmq_timer, &QTimer::timeout, this, [this, zmq_timer, zmq_start]() {
        // 5555 = REQ/REP control port (where `stream_info` round-trips).
        // 5556 is PUB/SUB — a REQ socket gets no reply there.
        if (mime::MimePhysicsProcess::probeRunner("tcp://localhost:5555",
                                                    1500)) {
            zmq_timer->stop();
            zmq_timer->deleteLater();
            if (noticeOverlay_) {
                noticeOverlay_->removeNotice(QStringLiteral("cloud-launch"));
            }
            finishSimulationStart();
            return;
        }
        const auto elapsed = std::chrono::steady_clock::now() - zmq_start;
        if (elapsed > std::chrono::minutes(20)) {
            zmq_timer->stop();
            zmq_timer->deleteLater();
            onCloudFailed(tr("waiting for MIME runner"),
                tr("Runner did not bind ZMQ on the cluster within 20 min. "
                   "Check `sky logs %1` from a host shell.")
                    .arg(cloudClusterName_));
            return;
        }
        // Every ~30s, refresh the notice with elapsed time so the user
        // knows the wait is intentional.
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                              elapsed).count();
        if (secs > 0 && secs % 30 < 2 && noticeOverlay_) {
            noticeOverlay_->addNotice(
                QStringLiteral("cloud-launch"),
                viewport::NoticeOverlay::Severity::Info,
                tr("Waiting for MIME runner (%1s)").arg(secs),
                tr("Pod cold-starts (apt + JAX install + JIT) usually take "
                   "5–10 min on a fresh cluster."));
        }
    });
    zmq_timer->start();
}

void MainWindow::onCloudFailed(const QString& stage, const QString& reason)
{
    spdlog::error("MainWindow: cloud launch failed at '{}' — {}",
                   stage.toStdString(), reason.toStdString());
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(
            QStringLiteral("cloud-launch"),
            viewport::NoticeOverlay::Severity::Error,
            tr("Cloud launch failed: %1").arg(stage),
            reason);
    }
    if (runConfigPanel_) {
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Cloud launch failed"));
    }
    // Best-effort cleanup — we may have a half-open tunnel or a half-up
    // cluster. Always close the tunnel; only `sky down` if the user asked.
    tearDownCloud(cloudStopOnDisconnect_);
    QMessageBox::critical(this, tr("Cloud launch failed"),
        tr("Stage: %1\n\n%2").arg(stage).arg(reason));
}

void MainWindow::tearDownCloud(bool stopCluster)
{
    if (cloudLogFollower_) {
        // Detach rather than terminate.  The pod's runner prints its
        // PERFORMANCE SUMMARY (sim/wall ratio, ms/step, etc.) from its
        // SIGTERM handler, which only fires *after* sky cancel/down
        // propagates to the pod.  If we SIGTERM the log follower up
        // front, those final lines never reach MimeConsolePanel —
        // exactly what the user just hit.  Let `sky logs --follow`
        // drain to natural exit (it stops when the job completes),
        // with a 30s wallclock cap as a force-cleanup fallback so a
        // hung sky doesn't leak the process forever.
        QProcess* follower = cloudLogFollower_;
        cloudLogFollower_ = nullptr;  // future tearDownCloud is a no-op
        QObject::connect(
            follower,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            follower, &QObject::deleteLater);
        QTimer::singleShot(30'000, follower, [follower]() {
            if (follower->state() == QProcess::Running) {
                spdlog::info("MainWindow: cloud log follower hadn't exited "
                              "30s after Stop — force-terminating");
                follower->terminate();
                if (!follower->waitForFinished(2000)) follower->kill();
            }
        });
    }
    if (sshTunnel_ && sshTunnel_->isOpen()) {
        sshTunnel_->close();
    }
    if (!cloudClusterName_.isEmpty() && cloudJobLauncher_) {
        // Always at least `sky cancel` (graceful — gives mime.runner
        // a chance to flush its PERFORMANCE SUMMARY).  If stopCluster
        // is set, the cancel is chained with `sky down` after an 8s
        // drain delay so sky logs can forward the runner's tail into
        // MimeConsolePanel before the pod disappears.
        cloudJobLauncher_->cancel(cloudClusterName_.toStdString(),
                                    stopCluster);
    }
    cloudClusterName_.clear();
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

    // For Cloud mode, tear down the SSH tunnel and (per the panel toggle)
    // the SkyPilot cluster.
    if (!cloudClusterName_.isEmpty()) {
        spdlog::info(
            "onSimulationStop: tearing down cloud — stopCluster={}",
            cloudStopOnDisconnect_);
        tearDownCloud(cloudStopOnDisconnect_);
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
    connect(&dialog, &SettingsDialog::noticesChanged, this,
            &MainWindow::applyNoticeSettings);
    connect(&dialog, &SettingsDialog::codeHighlightThemeChanged, this,
            [this](const QString& name) {
        if (scriptEditor_) scriptEditor_->setHighlightTheme(name);
        if (consoleWidget_) consoleWidget_->setHighlightTheme(name);
    });
    dialog.exec();
}

void MainWindow::applyNoticeSettings()
{
    if (!noticeOverlay_) return;
    QSettings s;
    s.beginGroup("notices");
    const bool enabled = s.value("enabled", true).toBool();
    const int opacity  = s.value("opacity", 85).toInt();
    s.endGroup();
    noticeOverlay_->setOverlayEnabled(enabled);
    noticeOverlay_->setOpacityPercent(opacity);
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
