#include "app/main_window.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QProcess>
#include <fstream>
#include <map>
#include <set>
#include <unordered_map>
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
#include "panels/mime_console_panel.h"
#include "panels/run_config_panel.h"
#include "panels/project_browser_panel.h"
#include "panels/graph_inspector_panel.h"
#include "panels/script_editor_panel.h"
#include "panels/live_inputs_panel.h"
#include "panels/plots_panel.h"
#include "panels/claims_panel.h"
#include "runs/run_folder.h"
#include "runs/visuals.h"
#include <nlohmann/json.hpp>
#include "viewport/camera_toolbar.h"
#include "viewport/camera_controller.h"
#include "viewport/local_viewport.h"
#include "viewport/notice_overlay.h"
#include "viewport/command_bar.h"
#include <QTimer>
#include <chrono>
#include "stubs/local_compute_backend.h"
#include "experiment/experiment_runner.h"
#include "live/binding_frames.h"
#include "live/live_client.h"
#include "live/live_physics_process.h"
#include "live/live_protocol.h"
#include "connection/connection_config.h"

#include <spdlog/spdlog.h>

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
    // Under the viewport, the commands a notice asks the user to run (MIME's
    // fetch command), selectable and copied in one click: the notice overlay
    // is input-transparent, so nothing on it can be selected or clicked.
    commandBar_ = new viewport::CommandBar(viewportContainer);
    viewportLayout->addWidget(commandBar_);
    connect(commandBar_, &viewport::CommandBar::copied, this, [this](const QString& command) {
        statusBar()->showMessage(tr("Copied: %1").arg(command), 8000);
    });
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
    if (readyTimer_) {
        readyTimer_->stop();
    }
    if (liveStatusPoll_) {
        liveStatusPoll_->stop();
    }
    // No request waits for MIME any longer: a stop still to send fails at once.
    if (connectingClient_) {
        connectingClient_->close();
        connectingClient_.reset();
    }
    if (liveClient_) {
        liveClient_->close();
    }
    if (simController_) {
        simController_->teardown();
    }
    liveProcess_ = nullptr;
    liveClient_.reset();
    if (serveProcess_ && serveProcess_->isRunning()) {
        serveProcess_->stop();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveLayout();
    cancelLiveConnect();
    if (liveSessionActive()) {
        // Stop the session first, its run folder written, without blocking the
        // window; it closes once the session has ended. Asked again: MIME is
        // waited for no longer.
        if (closeAfterEnd_) {
            endForced_ = true;
            if (liveClient_) liveClient_->close();
            if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->stopAsync(0);
        }
        closeAfterEnd_ = true;
        endLiveSession(tr("Live session stopped."));
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::openPath(const QString& path)
{
    const QFileInfo info(path);
    if (info.isDir() && runs::RunFolder::looksLikeRunFolder(info.absoluteFilePath().toStdString())) {
        return openRunFolder(info.absoluteFilePath());
    }
    if (info.isDir()) {
        return initExperiment(info.absoluteFilePath());
    }
    if (!info.exists()) {
        QMessageBox::warning(this, tr("Open Failed"), tr("No such file or folder:\n%1").arg(path));
        return false;
    }
    if (!openStage(info.absoluteFilePath())) {
        QMessageBox::warning(this, tr("Open Failed"), tr("Could not open scene file:\n%1").arg(path));
        return false;
    }
    return true;
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

    auto* openExpAction = fileMenu->addAction(tr("Open Saved &Experiment..."));
    openExpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    openExpAction->setToolTip(
        tr("Open a MIME 0.3 saved experiment (a folder Experiment.save or "
           "mime pull wrote): its stage and its files."));
    connect(openExpAction, &QAction::triggered,
            this, &MainWindow::onFileOpenExperiment);

    auto* openRunAction = fileMenu->addAction(tr("Open &Run Folder..."));
    openRunAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    openRunAction->setToolTip(tr("Open a MIME 0.3 run folder: its recording, its experiment, its outputs and "
                                 "MIME's claims about it."));
    connect(openRunAction, &QAction::triggered, this, &MainWindow::onOpenRunFolder);

    openLastRunAction_ = fileMenu->addAction(tr("Open &Last Session's Run Folder"));
    openLastRunAction_->setEnabled(false);
    connect(openLastRunAction_, &QAction::triggered, this, [this]() {
        if (!lastRunFolder_.isEmpty()) openRunFolder(lastRunFolder_);
    });

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

    simMenu->addSeparator();
    stepAction_ = simMenu->addAction(tr("St&ep One Chunk"));
    stepAction_->setShortcut(QKeySequence(Qt::Key_F8));
    stepAction_->setToolTip(tr("Step a paused live session by one chunk (mime.live/1 step)."));
    stepAction_->setEnabled(false);
    connect(stepAction_, &QAction::triggered, this, &MainWindow::onStepRequested);

    recordAction_ = simMenu->addAction(tr("&Record"));
    recordAction_->setCheckable(true);
    recordAction_->setToolTip(tr("Record the live session's stage into its run folder (recording.usdc)."));
    recordAction_->setEnabled(false);
    connect(recordAction_, &QAction::toggled, this, &MainWindow::onRecordToggled);

    replayAction_ = simMenu->addAction(tr("Re&play This Session"));
    replayAction_->setToolTip(tr("Replay the open run folder's live session from its command log "
                                 "(mime run --commands) into a new run folder."));
    replayAction_->setEnabled(false);
    connect(replayAction_, &QAction::triggered, this, &MainWindow::onReplayRequested);

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
    viewMenu->addAction(projectBrowser_->toggleViewAction());
    viewMenu->addAction(graphInspector_->toggleViewAction());
    viewMenu->addAction(scriptEditor_->toggleViewAction());
    viewMenu->addAction(liveInputsPanel_->toggleViewAction());
    viewMenu->addAction(plotsPanel_->toggleViewAction());
    viewMenu->addAction(claimsPanel_->toggleViewAction());

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

    // Bottom docks: timeline, console, MIME console
    timelinePanel_ = new panels::TimelinePanel(*simController_, this);
    addDockWidget(Qt::BottomDockWidgetArea, timelinePanel_);

    consoleWidget_ = new panels::ConsoleWidget(*scriptEngine_, this);
    addDockWidget(Qt::BottomDockWidgetArea, consoleWidget_);

    mimeConsole_ = new panels::MimeConsolePanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, mimeConsole_);

    // Phase G panels
    graphInspector_ = new panels::GraphInspectorPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, graphInspector_);

    scriptEditor_ = new panels::ScriptEditorPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, scriptEditor_);

    liveInputsPanel_ = new panels::LiveInputsPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, liveInputsPanel_);

    plotsPanel_ = new panels::PlotsPanel(this);
    addDockWidget(Qt::BottomDockWidgetArea, plotsPanel_);
    claimsPanel_ = new panels::ClaimsPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, claimsPanel_);

    // The plots beside the timeline, not behind it: playback's cursor moves
    // through them while the timeline plays. Split before the timeline joins
    // a tab group (a dock split from a tabbed one becomes another tab).
    splitDockWidget(timelinePanel_, plotsPanel_, Qt::Horizontal);
    resizeDocks({timelinePanel_, plotsPanel_}, {1, 2}, Qt::Horizontal);

    // Tab bottom panels together
    tabifyDockWidget(timelinePanel_, consoleWidget_);
    tabifyDockWidget(consoleWidget_, mimeConsole_);
    tabifyDockWidget(mimeConsole_, graphInspector_);
    tabifyDockWidget(graphInspector_, scriptEditor_);
    tabifyDockWidget(liveInputsPanel_, claimsPanel_);
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
    // Live inputs → the session's `set` (from the controller's next tick).
    connect(liveInputsPanel_, &panels::LiveInputsPanel::setRequested,
            this, [this](const nlohmann::json& values) {
        if (!liveSessionActive() || !simController_->isRunning()) {
            statusBar()->showMessage(tr("No live session: nothing to set"));
            return;
        }
        simController_->sendParameters(values);  // posted: its outcome comes to onLiveOutcome
    });
    connect(timelinePanel_, &panels::TimelinePanel::playbackSecondsChanged,
            plotsPanel_, &panels::PlotsPanel::setCursor);
    connect(liveInputsPanel_, &panels::LiveInputsPanel::rebuildRequested, this, [this]() {
        // A physical setting changed in the saved folder: a new session, once
        // this one has ended.
        if (experimentDir_.empty()) return;
        if (liveSessionActive()) {
            relaunchAfterEnd_ = true;
            endLiveSession(tr("Live session stopped for a rebuild."));
            return;
        }
        onSimulationStart();
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

    // A recording in a run folder is its run's: open the folder with it, so
    // its experiment, outputs and claims come along.
    const std::string run = runs::RunFolder::runFolderOf(filePath.toStdString());
    if (!run.empty() && QFileInfo(filePath).fileName() == QStringLiteral("recording.usdc")) {
        openRunFolder(QString::fromStdString(run));
        return;
    }

    if (!openStage(filePath)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not open scene file:\n%1").arg(filePath));
    }
}

bool MainWindow::openStage(const QString& filePath)
{
    // Leave playback of the previous stage, if any, before swapping stages.
    if (playbackMode_) {
        playbackMode_ = false;
        timelinePanel_->enterSimulationMode();
        viewportWidget_->resetTimeCode();
        disconnect(timelinePanel_, &panels::TimelinePanel::timeCodeChanged,
                   viewportWidget_, &viewport::ViewportWidget::setTimeCode);
    }
    if (sceneMgr_->isLoaded()) {
        primSelection_->clear();
        sceneMgr_->closeScene();
    }
    // What MIME's visuals said was of the stage closed (the session's or the
    // run folder's say it again of theirs).
    clearVisuals();

    if (!sceneMgr_->loadScene(filePath.toStdString())) {
        return false;
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
    return true;
}

bool MainWindow::initExperiment(const QString& dirStr)
{
    if (dirStr.isEmpty()) return false;
    const QDir dir(dirStr);

    // A MIME 0.3 saved experiment: experiment.mime.yaml, mime.lock and its
    // stage, scene/scene.usda (Experiment.save, mime pull). MIME 0.2's
    // experiment folders (experiment.yaml, physics/params.py, a hand-made
    // world.usda) are retired with the 0.2 runner.
    if (!QFile::exists(dir.filePath(QStringLiteral("experiment.mime.yaml")))) {
        const bool zeroTwo = QFile::exists(dir.filePath(QStringLiteral("experiment.yaml")));
        spdlog::warn("initExperiment: no experiment.mime.yaml under {}{}",
                     dirStr.toStdString(), zeroTwo ? " (a MIME 0.2 experiment folder)" : "");
        QMessageBox::warning(this, tr("Not a saved experiment"),
            zeroTwo
                ? tr("%1 is a MIME 0.2 experiment folder (experiment.yaml), "
                     "which MICROROBOTICA no longer runs.\n\n"
                     "Open a MIME 0.3 saved experiment instead: a folder "
                     "written by Experiment.save or mime pull, holding "
                     "experiment.mime.yaml and scene/scene.usda.").arg(dirStr)
                : tr("No experiment.mime.yaml in:\n%1\n\n"
                     "A MIME 0.3 saved experiment is a folder written by "
                     "Experiment.save or mime pull.").arg(dirStr));
        return false;
    }

    if (experimentDir_ == dirStr.toStdString() && sceneMgr_->isLoaded()) {
        return true;
    }
    spdlog::info("initExperiment: saved experiment {}", dirStr.toStdString());
    runFolder_.clear();
    if (replayAction_) replayAction_->setEnabled(false);
    if (plotsPanel_) plotsPanel_->clear();
    if (claimsPanel_) claimsPanel_->clear();
    if (simController_->isRunning()) {
        simController_->stop();
    }
    experimentDir_ = dirStr.toStdString();

    // The saved folder's stage, written by MIME from the experiment's
    // declarations (the same bytes for the same declarations).
    const QString stage = dir.filePath(QStringLiteral("scene/scene.usda"));
    if (!QFile::exists(stage)) {
        spdlog::warn("initExperiment: no scene/scene.usda under {}", dirStr.toStdString());
    } else if (!openStage(stage)) {
        QMessageBox::warning(this, tr("Scene load failed"),
            tr("The saved experiment's stage failed to load:\n%1").arg(stage));
    }

    // Mirror the path into the Run Configuration panel. setExperimentDir does
    // not emit experimentDirChanged, so this won't re-enter initExperiment.
    if (runConfigPanel_) {
        runConfigPanel_->setExperimentDir(experimentDir_);
    }
    if (projectBrowser_) {
        projectBrowser_->setExperimentDir(experimentDir_);
    }
    // The live inputs come from the session (mime.live/1's hello), not from
    // the folder: nothing to show until a session runs.
    if (liveInputsPanel_) {
        liveInputsPanel_->clearAll();
        // The experiment's settings, read-only, as its lock resolved them.
        std::ifstream lock(dir.filePath(QStringLiteral("mime.lock")).toStdString());
        if (lock) {
            try {
                const auto j = nlohmann::json::parse(lock);
                if (j.contains("resolved") && j["resolved"].contains("entries")) {
                    liveInputsPanel_->showSettings(j["resolved"]["entries"]);
                }
            } catch (const std::exception& e) {
                spdlog::warn("initExperiment: mime.lock not read: {}", e.what());
            }
        }
    }

    statusBar()->showMessage(
        tr("Saved experiment opened: %1").arg(QFileInfo(dirStr).fileName()));
    if (runConfigPanel_) {
        runConfigPanel_->setConnectionStatus(tr("Opened — not running"));
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

    // Resume from pause
    if (paused_ && simController_->isRunning()) {
        paused_ = false;
        simController_->setPaused(false);  // a live session: `start`
        timelinePanel_->resumePolling();
        viewportWidget_->setContinuousRendering(true);
        statusBar()->showMessage(tr("Simulation resumed"));
        if (cameraToolbar_) cameraToolbar_->onSimStarted();
        return;
    }

    if (simController_->isRunning() || liveSessionActive()) return;
    if (readyTimer_ && readyTimer_->isActive()) return;  // a session is starting
    if (connectingClient_) return;                       // hello is on its way

    const connection::ConnectionMode mode = runConfigPanel_
        ? runConfigPanel_->selectedMode()
        : connection::ConnectionMode::Local;

    // Manual endpoint: a session already serving (a remote mime serve
    // through an SSH tunnel, or one started by hand).
    if (mode == connection::ConnectionMode::Manual) {
        liveOut_.clear();  // its run folder is where it was told to write it
        connectLiveSession(QString::fromStdString(runConfigPanel_->commandsEndpoint()),
                           QString::fromStdString(runConfigPanel_->framesEndpoint()), manualSecurity());
        return;
    }
    // Local, with a saved experiment open: `mime serve` it.
    if (!experimentDir_.empty()) {
        startLocalSession();
        return;
    }

    if (!sceneMgr_->isLoaded()) {
        QMessageBox::information(this, tr("No Scene"),
            tr("Open a saved experiment (or a USD scene) before starting."));
        return;
    }
    finishSimulationStart();
}

void MainWindow::finishSimulationStart()
{
    // No MIME session: the stub backend, kept so the viewport demo works
    // without one.
    core::PhysicsConfig config;
    simController_->setComputeBackend(std::make_unique<stubs::LocalComputeBackend>());
    config.actorToPrimPath["robot"] = "/World/Actors/UMR";
    spdlog::info("finishSimulationStart: no MIME session → LocalComputeBackend (stub)");
    simController_->launchPhysics(config);
    paused_ = false;
    statusBar()->showMessage(tr("Simulation started"));
}

// ---------------------------------------------------------------------------
// Live sessions: MIME 0.3's `mime serve`, mime.live/1
// ---------------------------------------------------------------------------

bool MainWindow::liveSessionActive() const
{
    return liveClient_ != nullptr;
}

QString MainWindow::mimeCommand() const
{
    const QString fromEnv = QProcessEnvironment::systemEnvironment().value(QStringLiteral("MICROROBOTICA_MIME"));
    if (!fromEnv.isEmpty()) return fromEnv;
    QSettings s;
    const QString fromSettings = s.value(QStringLiteral("mime/command")).toString().trimmed();
    return fromSettings.isEmpty() ? QStringLiteral("mime") : fromSettings;
}

QString MainWindow::runsDir() const
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("mime/runs_dir")).toString().trimmed();
    return dir.isEmpty() ? QDir::homePath() + QStringLiteral("/MICROROBOTICA/runs") : dir;
}

void MainWindow::startLocalSession()
{
    const QString folder = QString::fromStdString(experimentDir_);
    readyDir_ = std::make_unique<QTemporaryDir>();
    if (!readyDir_->isValid()) {
        QMessageBox::critical(this, tr("Live session"), tr("Could not make a temporary folder for mime serve."));
        return;
    }
    const QString name = QFileInfo(folder).fileName();
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
    QDir().mkpath(runsDir());

    experiment::ExperimentRunner::ServeCommand command;
    command.program = mimeCommand();
    command.folder = folder;
    command.readyFile = readyDir_->filePath(QStringLiteral("ready.json"));
    command.out = QDir(runsDir()).filePath(QStringLiteral("%1-live-%2").arg(name, stamp));

    if (!serveProcess_) {
        serveProcess_ = std::make_unique<experiment::ExperimentRunner>(this);
        connect(serveProcess_.get(), &experiment::ExperimentRunner::stdoutChunk,
                mimeConsole_, &panels::MimeConsolePanel::appendStdout);
        connect(serveProcess_.get(), &experiment::ExperimentRunner::stderrChunk,
                mimeConsole_, &panels::MimeConsolePanel::appendStderr);
        connect(serveProcess_.get(), &experiment::ExperimentRunner::processStopped,
                this, &MainWindow::onServeProcessEnded);
    }
    mimeConsole_->appendStdout(QStringLiteral("$ %1 %2\n")
        .arg(command.program, experiment::ExperimentRunner::serveArguments(command).join(QLatin1Char(' ')))
        .toUtf8());
    if (!serveProcess_->startServe(command)) {
        QMessageBox::critical(this, tr("mime serve did not start"),
            tr("Could not run \"%1\".\n\nInstall MIME 0.3, or name its `mime` command "
               "in File → Settings → MIME command (or MICROROBOTICA_MIME).").arg(command.program));
        return;
    }

    runConfigPanel_->setLaunchEnabled(false);
    runConfigPanel_->setConnectionStatus(tr("Starting mime serve..."));
    statusBar()->showMessage(tr("Starting mime serve for %1...").arg(name));
    if (noticeOverlay_) {
        noticeOverlay_->addNotice(QStringLiteral("mime-serve"), viewport::NoticeOverlay::Severity::Info,
            tr("Starting MIME"),
            tr("mime serve is loading and building %1, then compiling its first chunk. "
               "You can keep navigating the viewport.").arg(name));
    }

    // Readiness: mime serve writes its ready file once its sockets listen.
    if (!readyTimer_) {
        readyTimer_ = new QTimer(this);
        readyTimer_->setInterval(100);
    }
    readyTimer_->disconnect();
    const QString readyFile = command.readyFile;
    const auto started = std::chrono::steady_clock::now();
    connect(readyTimer_, &QTimer::timeout, this, [this, readyFile, started]() {
        auto fail = [this](const QString& why) {
            readyTimer_->stop();
            if (noticeOverlay_) noticeOverlay_->removeNotice(QStringLiteral("mime-serve"));
            mimeConsole_->flush();
            runConfigPanel_->setLaunchEnabled(true);
            runConfigPanel_->setConnectionStatus(tr("Idle"));
            statusBar()->showMessage(why);
            mimeConsole_->raise();
            QMessageBox::warning(this, tr("Live session"), why);
        };
        if (!serveProcess_ || !serveProcess_->isRunning()) {
            fail(tr("mime serve stopped before it was ready: see MIME Output for why."));
            return;
        }
        QFile file(readyFile);
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            try {
                const auto ready = live::parseReadyFile(file.readAll().toStdString());
                liveOut_ = QString::fromStdString(ready.out);
                readyTimer_->stop();
                if (noticeOverlay_) noticeOverlay_->removeNotice(QStringLiteral("mime-serve"));
                connectLiveSession(QString::fromStdString(ready.commands), QString::fromStdString(ready.frames),
                                   ready.security);
                return;
            } catch (const live::ProtocolError&) {
                // Written but not yet whole: look again at the next tick.
            }
        }
        if (std::chrono::steady_clock::now() - started > std::chrono::minutes(5)) {
            if (serveProcess_) serveProcess_->stopAsync(0);
            fail(tr("mime serve was not ready within 5 minutes."));
        }
    });
    readyTimer_->start();
}

live::Security MainWindow::manualSecurity() const
{
    QSettings s;
    return live::securityOf(s.value(QStringLiteral("mime/mechanism")).toString().toStdString(),
                            s.value(QStringLiteral("mime/server_key")).toString().trimmed().toStdString());
}

void MainWindow::connectLiveSession(const QString& commands, const QString& frames, const live::Security& security)
{
    // A mechanism this client cannot speak is refused, never replaced by the
    // null mechanism (mime.live/1's rule; the client refuses it too).
    if (const auto refusal = live::mechanismRefusal(security)) {
        const QString why = QString::fromStdString(*refusal);
        mimeConsole_->appendStderr((QStringLiteral("[session] %1: ").arg(commands) + why + QLatin1Char('\n')).toUtf8());
        if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->stopAsync(0);
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Idle"));
        statusBar()->showMessage(tr("Not connected: the session's security mechanism is not supported"));
        QMessageBox::warning(this, tr("Live session"), tr("Cannot join the session at %1:\n%2").arg(commands, why));
        return;
    }

    // Each command waits as long as MIME may take to answer it: hello, status
    // and pause at once (a few seconds), even while a chunk compiles; set,
    // record, pace, start and step at the end of the chunk in hand (30 s, a
    // first chunk's compile); a stop between chunks once its run folder is
    // written (LiveClient::Timeouts).
    // hello is asked on the client's commands lane; the session is taken up
    // when it answers (onLiveConnected), the window answering meanwhile.
    auto client = std::make_shared<live::LiveClient>(commands.toStdString(), frames.toStdString(),
                                                     live::LiveClient::Timeouts{}, security);
    connectingClient_ = client;
    const quint64 session = ++liveSessionId_;
    runConfigPanel_->setLaunchEnabled(false);
    runConfigPanel_->setConnectionStatus(tr("Connecting to %1...").arg(commands));
    client->connectAsync([this, session, commands](const live::LiveOutcome& outcome) {
        QMetaObject::invokeMethod(this, [this, session, commands, outcome]() {
            onLiveConnected(session, commands, outcome);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::cancelLiveConnect()
{
    if (!connectingClient_) return;
    ++liveSessionId_;  // its hello, if it comes, is dropped
    connectingClient_->close();
    connectingClient_.reset();
    if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->stopAsync(0);
    runConfigPanel_->setLaunchEnabled(true);
    runConfigPanel_->setConnectionStatus(tr("Idle"));
}

void MainWindow::onLiveConnected(quint64 session, const QString& commands, const live::LiveOutcome& outcome)
{
    if (session != liveSessionId_ || !connectingClient_) return;  // cancelled meanwhile
    const std::shared_ptr<live::LiveClient> client = std::move(connectingClient_);
    connectingClient_.reset();
    if (outcome.error) {
        const QString why = QString::fromStdString(outcome.errorText());
        spdlog::error("connectLiveSession: {}", outcome.errorText());
        mimeConsole_->appendStderr((QStringLiteral("hello at %1: ").arg(commands) + why + QLatin1Char('\n')).toUtf8());
        // Never started: a Local mime serve goes (SIGTERM, which it answers by stopping).
        if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->stopAsync(0);
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Idle"));
        QString hint;
        if (liveOut_.isEmpty()) {
            // A Manual endpoint (a Local session has its ready file's run
            // folder): a server speaking another mechanism than the settings
            // name does not answer at all.
            hint = tr("\n\nIf that server uses CurveZMQ, set its mechanism and public key in "
                      "File → Settings → Manual endpoint security.");
        }
        QMessageBox::warning(this, tr("Live session"),
            tr("No mime.live/1 session answered at %1:\n%2").arg(commands, why) + hint);
        return;
    }
    const live::Hello& hello = client->hello();
    if (!applyHello(hello)) {
        // Never started: a Local mime serve goes; a Manual one is left as it was.
        if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->stopAsync(0);
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Idle"));
        return;
    }

    // The binding, in the stage's own units (MIME's frames are in metres).
    live::StageUnits units;
    units.metersPerUnit = sceneMgr_->metersPerUnit();
    units.vectorMetresPerUnit = sceneMgr_->vectorMetresPerUnit();
    liveFrames_ = std::make_shared<const live::BindingFrames>(hello.binding, hello.schema, units);
    for (const auto& missing : liveFrames_->unbound()) {
        mimeConsole_->appendStderr(("[session] bound but not in the frames: " + missing + "\n").c_str());
    }
    liveClient_ = client;

    // Plots and claims: every scalar a frame carries (observation outputs,
    // flags), sample by sample, from the frames thread. The build report
    // names the flags; without one (an older MIME), their field names do.
    std::set<std::string> reportFlags;
    if (hello.report.report) {
        for (const auto& f : hello.report.report->flags) reportFlags.insert(f.path);
    }
    std::vector<panels::PlotsPanel::FieldInfo> fields;
    std::vector<std::string> flagPaths;
    for (const auto& f : hello.schema.fields) {
        if (f.count() != 1) continue;
        const bool flag = hello.report.report
            ? reportFlags.count(f.path) > 0
            : (f.field.find("fired") != std::string::npos || f.field.rfind("beyond", 0) == 0);
        fields.push_back({f.path, f.unit, flag});
        if (flag) flagPaths.push_back(f.path);
    }
    plotsPanel_->beginLive(fields);
    claimsPanel_->beginLive(hello.experiment, flagPaths, hello.report);
    runFolder_.clear();
    sessionRunFolder_.clear();
    liveStopPending_ = false;
    statusInFlight_ = false;
    liveEnding_ = false;
    endForced_ = false;
    liveLine_ = LiveLine{};
    liveLineLogged_.clear();
    liveLine_.state = hello.status.state;
    liveLine_.step = hello.status.step;
    liveLine_.time = hello.status.time;
    liveLine_.recording = hello.status.recording;
    replayAction_->setEnabled(false);

    auto backend = std::make_unique<live::LiveComputeBackend>(
        client, liveFrames_, [this, flagPaths, session](live::LivePhysicsProcess& process) {
            liveProcess_ = &process;
            // Each command's outcome, from the client's commands lane, to the
            // UI thread, in the order the commands were posted.
            process.setCommandObserver([this, session](const live::LiveOutcome& outcome) {
                QMetaObject::invokeMethod(this, [this, session, outcome]() {
                    if (session == liveSessionId_ && liveClient_) onLiveOutcome(outcome);
                }, Qt::QueuedConnection);
            });
            auto reported = std::make_shared<std::set<std::string>>();
            process.setFrameObserver([this, flagPaths, reported](const live::LiveFrame& frame) {
                std::unordered_map<std::string, double> scalars;
                for (const auto& [path, values] : frame.values) {
                    if (values.size() == 1) scalars.emplace(path, values.front());
                }
                plotsPanel_->pushSample(frame.time, scalars);
                std::map<std::string, double> raised;
                for (const auto& path : flagPaths) {
                    const auto it = scalars.find(path);
                    if (it != scalars.end() && it->second > 0.5 && reported->insert(path).second) {
                        raised[path] = it->second;
                    }
                }
                if (!raised.empty()) {
                    const double t = frame.time;
                    QMetaObject::invokeMethod(claimsPanel_, [this, t, raised]() { claimsPanel_->liveFlags(t, raised); },
                                              Qt::QueuedConnection);
                }
            });
        });
    simController_->setComputeBackend(std::move(backend));
    simController_->launchPhysics(liveFrames_->physicsConfig());  // posts `start`

    paused_ = false;
    stepAction_->setEnabled(true);
    recordAction_->setEnabled(true);
    recordAction_->blockSignals(true);
    recordAction_->setChecked(hello.status.recording);
    recordAction_->blockSignals(false);
    viewportWidget_->setContinuousRendering(true);
    if (cameraToolbar_) cameraToolbar_->onSimStarted();
    runConfigPanel_->setLaunchEnabled(false);
    showLiveLine();
    statusBar()->showMessage(tr("Live session: %1 (MIME %2, %3 frames of %4 bytes a chunk of %5 steps)")
        .arg(QString::fromStdString(hello.experiment), QString::fromStdString(hello.mimeVersion))
        .arg(hello.schema.fields.size()).arg(hello.schema.frameBytes).arg(hello.chunkSteps));
}

bool MainWindow::applyHello(const live::Hello& hello)
{
    mimeConsole_->appendStdout(QStringLiteral("[session] %1, MIME %2: %3 bound prims, %4 fields a frame, "
                                              "chunk of %5 base steps of %6 s\n")
        .arg(QString::fromStdString(hello.experiment), QString::fromStdString(hello.mimeVersion))
        .arg(hello.binding.size()).arg(hello.schema.fields.size()).arg(hello.chunkSteps).arg(hello.dt)
        .toUtf8());

    // The stage: the served saved folder's own, when it is on this machine;
    // for a remote session, the same saved experiment opened here.
    const QString stage = QString::fromStdString(hello.stage);
    const bool here = !stage.isEmpty() && QFile::exists(stage);
    if (here) {
        const QString open = QString::fromStdString(sceneMgr_->filePath());
        if (playbackMode_ || !sceneMgr_->isLoaded()
            || QFileInfo(open).canonicalFilePath() != QFileInfo(stage).canonicalFilePath()) {
            if (!openStage(stage)) {
                QMessageBox::warning(this, tr("Live session"), tr("The session's stage did not load:\n%1").arg(stage));
                return false;
            }
        }
    } else if (!sceneMgr_->isLoaded() || experimentDir_.empty() || playbackMode_) {
        QMessageBox::warning(this, tr("Live session"),
            tr("The session's stage is not on this machine:\n%1\n\n"
               "Open the same saved experiment here (File → Open Saved Experiment), then connect.")
                .arg(stage));
        return false;
    } else {
        // A remote session: the saved experiment open here stands in for the
        // server's, if it is the same experiment, by its input hash.
        std::string localInput;
        std::ifstream lock(experimentDir_ + "/mime.lock");
        try {
            const auto j = nlohmann::json::parse(lock);
            localInput = j.at("hashes").value("input", std::string());
        } catch (const std::exception&) {}
        const auto served = hello.hashes.find("input");
        if (served == hello.hashes.end() || localInput.empty() || served->second != localInput) {
            QMessageBox::warning(this, tr("Live session"),
                tr("The session serves %1, and the saved experiment open here is another (or its lock "
                   "is unreadable):\nserved %2\nhere %3\n\nOpen the saved folder the session serves "
                   "(or a mime pull of it), then connect.")
                    .arg(QString::fromStdString(hello.experiment),
                         QString::fromStdString(served == hello.hashes.end() ? std::string("?") : served->second),
                         QString::fromStdString(localInput.empty() ? std::string("?") : localInput)));
            return false;
        }
    }

    // MIME's visuals: the overlay that draws meshes MIME does not ship, or
    // what is missing and how to fetch it (the arm then draws as bones), for
    // the folder open here. hello's are of the server's asset cache: a
    // session whose stage is not on this machine asks this machine's MIME.
    clearVisuals();
    if (!here) {
        requestVisualsView(QString::fromStdString(experimentDir_), QString(), true);
    } else if (hello.visuals) {
        const auto& v = *hello.visuals;
        const bool overlay = v.overlay && QFile::exists(QString::fromStdString(*v.overlay))
            && sceneMgr_->setOverlayLayer(*v.overlay);
        showVisuals(overlay ? v.drawn : 0, v.missing, v.fetch, v.noticeLines(),
                    liveOut_.isEmpty() ? tr("connect again") : tr("launch the session again"),
                    QStringLiteral("session"), true);
    }

    if (hello.report.report) {
        const auto& report = *hello.report.report;
        long declared = 0;
        for (const auto& v : report.validity) declared += v.declaredOnly() ? 1 : 0;
        mimeConsole_->appendStdout(QStringLiteral("[session] build report: %1 validity lines (%2 declared, not yet "
                                                  "checked), %3 watched after the run, %4 flags\n")
            .arg(report.validity.size()).arg(declared).arg(report.watches.size()).arg(report.flags.size()).toUtf8());
    } else if (hello.report.unreadable) {
        mimeConsole_->appendStdout(QStringLiteral("[session] the build report is of format %1, which this "
                                                  "MICROROBOTICA does not read\n")
            .arg(QString::fromStdString(*hello.report.unreadable)).toUtf8());
    }

    if (graphInspector_ && hello.topology.is_object()) {
        graphInspector_->loadFromJson(hello.topology);
    }
    if (liveInputsPanel_) {
        liveInputsPanel_->showSession(hello.controller, hello.liveInputs);
    }
    return true;
}

void MainWindow::onLiveReply(const live::Reply& reply)
{
    const QString command = QString::fromStdString(reply.command);
    if (!reply.ok) {
        mimeConsole_->appendRefusals(tr("%1 is refused").arg(command), reply.errors);
        const QString code = reply.errors.empty() ? QString() : QString::fromStdString(reply.errors.front().code);
        statusBar()->showMessage(tr("MIME refused %1: %2 (see MIME Output)").arg(command, code));
    }
    if (reply.command == "record" && reply.ok) {
        recordAction_->blockSignals(true);
        recordAction_->setChecked(reply.status.recording);
        recordAction_->blockSignals(false);
    }
    if (reply.command == "stop" && reply.folder) {
        sessionRunFolder_ = QString::fromStdString(*reply.folder);
    }
    if (reply.stopPending()) {
        liveStopPending_ = true;
    }
    if (!reply.ok || reply.status.state.empty()) return;

    // State, activity and pending from the reply; step and time too, unless a
    // frame has already shown a later step (while a chunk runs, a reply's step
    // is the one the chunk started at).
    liveLine_.state = reply.status.state;
    liveLine_.activity = reply.status.activity;
    liveLine_.pending = reply.status.pending;
    liveLine_.recording = reply.status.recording;
    if (reply.status.step >= liveLine_.step) {
        liveLine_.step = reply.status.step;
        liveLine_.time = reply.status.time;
    }
    showLiveLine();

    // Poll status while the first chunk compiles, or a pause waits for the
    // chunk in hand (entry 21); once after start or step, whose first chunk
    // may compile next. Never after a stop is taken: the server answers no
    // status once it has stopped (entry 23).
    const bool stopping = liveStopPending_ || reply.command == "stop" || reply.status.isPending("stop");
    const bool follow = reply.command == "start" || reply.command == "step";
    const bool poll = !stopping && (follow || reply.status.compiling() || reply.status.isPending("pause"));
    if (!liveStatusPoll_) {
        liveStatusPoll_ = new QTimer(this);
        liveStatusPoll_->setInterval(250);
        connect(liveStatusPoll_, &QTimer::timeout, this, [this]() {
            if (!liveClient_ || liveClient_->closed() || liveStopPending_ || liveEnding_) {
                liveStatusPoll_->stop();
                return;
            }
            if (statusInFlight_) return;  // one at a time
            statusInFlight_ = true;
            const quint64 session = liveSessionId_;
            // The status lane: never queued behind a command waiting for the
            // chunk in hand.
            liveClient_->postStatus([this, session](const live::LiveOutcome& outcome) {
                QMetaObject::invokeMethod(this, [this, session, outcome]() {
                    if (session != liveSessionId_ || !liveClient_) return;
                    statusInFlight_ = false;
                    if (outcome.error) {
                        liveStatusPoll_->stop();
                        statusBar()->showMessage(tr("status: %1").arg(QString::fromStdString(outcome.errorText())));
                        return;
                    }
                    if (!liveEnding_) onLiveReply(*outcome.reply);
                }, Qt::QueuedConnection);
            });
        });
    }
    if (poll && !liveStatusPoll_->isActive()) {
        liveStatusPoll_->start();
    } else if (!poll) {
        liveStatusPoll_->stop();
    }
}

void MainWindow::showLiveLine()
{
    if (!runConfigPanel_ || !liveSessionActive()) return;
    QString line = tr("Live: %1").arg(QString::fromStdString(liveLine_.state.empty() ? std::string("connected")
                                                                                     : liveLine_.state));
    if (liveLine_.activity == "compiling") {
        line += tr(", compiling");
    }
    for (const auto& command : liveLine_.pending) {
        line += tr(", %1 pending").arg(QString::fromStdString(command));
    }
    if (line != liveLineLogged_) {  // the log says each change of state, activity or what is pending
        liveLineLogged_ = line;
        spdlog::info("Live session status: {} (step {})", line.toStdString(), liveLine_.step);
    }
    line += tr(", step %1, %2 s").arg(liveLine_.step).arg(liveLine_.time, 0, 'f', 3);
    if (liveLine_.recording) line += tr(", recording");
    runConfigPanel_->setConnectionStatus(line);
}

void MainWindow::onLiveOutcome(const live::LiveOutcome& outcome)
{
    const std::string command = outcome.request.value("command", std::string());
    if (command == "set") {
        liveInputsPanel_->markSet(outcome.request.value("values", nlohmann::json()), outcome.ok());
    }
    if (outcome.error) {
        const QString why = QString::fromStdString(outcome.errorText());
        mimeConsole_->appendStderr((QStringLiteral("[session] ") + why + QLatin1Char('\n')).toUtf8());
        statusBar()->showMessage(tr("%1: %2").arg(QString::fromStdString(command), why));
        return;
    }
    onLiveReply(*outcome.reply);
}

void MainWindow::postLive(const nlohmann::json& request, std::function<void(const live::LiveOutcome&)> after)
{
    if (!liveClient_ || liveEnding_) return;
    const quint64 session = liveSessionId_;
    liveClient_->post(request, [this, session, after = std::move(after)](const live::LiveOutcome& outcome) {
        QMetaObject::invokeMethod(this, [this, session, after, outcome]() {
            if (session != liveSessionId_ || !liveClient_) return;
            onLiveOutcome(outcome);
            if (after) after(outcome);
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onStepRequested()
{
    if (!liveSessionActive() || liveEnding_) return;
    if (!paused_) {
        statusBar()->showMessage(tr("Pause the session to step it"));
        return;
    }
    // Answered once the chunk is stepped (a first chunk compiles first): the
    // window answers meanwhile, and the stepped frame is shown when it comes.
    postLive(live::stepRequest(1), [this](const live::LiveOutcome&) { simController_->requestNextFrame(); });
}

void MainWindow::onRecordToggled(bool on)
{
    if (!liveSessionActive() || liveEnding_) return;
    postLive(live::recordRequest(on));
}

void MainWindow::endLiveSession(const QString& why)
{
    if (!liveSessionActive() || liveEnding_) return;
    liveEnding_ = true;
    liveEndWhy_ = why;
    if (liveStatusPoll_) liveStatusPoll_->stop();
    stepAction_->setEnabled(false);
    recordAction_->setEnabled(false);
    runConfigPanel_->setConnectionStatus(tr("Stopping the session..."));
    statusBar()->showMessage(tr("Stopping the live session..."));
    // mime serve exits on its own once stopped: that exit is no crash.
    if (serveProcess_ && serveProcess_->isRunning()) serveProcess_->expectExit();
    const quint64 session = liveSessionId_;
    if (liveProcess_ && simController_->isRunning()) {
        // `stop`, after any command still on its way. Between chunks it
        // replies once the run folder is written; taken while a chunk runs,
        // it completes with the final frame.
        liveProcess_->requestStop([this, session]() {
            QMetaObject::invokeMethod(this, [this, session]() {
                if (session == liveSessionId_) onLiveStopped();
            }, Qt::QueuedConnection);
        });
    } else {
        onLiveStopped();  // the stream had already ended
    }
}

void MainWindow::onLiveStopped()
{
    if (!liveEnding_ || awaitingServeExit_) return;
    simController_->stop();  // the stream has ended: joins its worker, logs its summary
    liveProcess_ = nullptr;
    if (serveProcess_ && serveProcess_->isRunning()) {
        // A pending stop's run folder is written after the final frame, and
        // any stop's process exits after its folder: wait for that exit.
        awaitingServeExit_ = true;
        runConfigPanel_->setConnectionStatus(tr("Waiting for mime serve to finish its run folder..."));
        serveProcess_->stopAsync(endForced_ ? 0 : 60000);
        return;  // onServeProcessEnded completes the end
    }
    completeLiveSession();
}

void MainWindow::completeLiveSession()
{
    awaitingServeExit_ = false;
    mimeConsole_->flush();
    // A pending stop's reply names no run folder: a Local session's is where
    // its ready file said it would write, once the process has written it.
    if (sessionRunFolder_.isEmpty() && !liveOut_.isEmpty()
        && runs::RunFolder::looksLikeRunFolder(liveOut_.toStdString())) {
        sessionRunFolder_ = liveOut_;
    }
    if (!sessionRunFolder_.isEmpty()) {
        lastRunFolder_ = sessionRunFolder_;
    }
    const bool folderUnknown = sessionRunFolder_.isEmpty() && liveStopPending_;
    sessionRunFolder_.clear();
    liveOut_.clear();
    liveProcess_ = nullptr;
    liveClient_.reset();
    liveFrames_.reset();
    liveEnding_ = false;
    paused_ = false;
    stepAction_->setEnabled(false);
    recordAction_->setEnabled(false);
    liveInputsPanel_->endSession();
    viewportWidget_->setContinuousRendering(false);
    if (cameraToolbar_) cameraToolbar_->onSimStopped();
    runConfigPanel_->setLaunchEnabled(true);
    runConfigPanel_->setConnectionStatus(tr("Idle"));
    QString message = liveEndWhy_;
    if (folderUnknown) {
        message += tr(" It stopped at the end of a chunk; its run folder is the one mime serve was told to write "
                      "(--out).");
    } else if (!lastRunFolder_.isEmpty()) {
        openLastRunAction_->setEnabled(true);
        message += tr(" Its run folder: %1 (File → Open Last Session's Run Folder)").arg(lastRunFolder_);
        mimeConsole_->appendStdout(("[session] run folder: " + lastRunFolder_ + QLatin1Char('\n')).toUtf8());
    }
    statusBar()->showMessage(message);
    spdlog::info("Live session ended: {}", message.toStdString());

    if (closeAfterEnd_) {
        QTimer::singleShot(0, this, &QWidget::close);
    } else if (relaunchAfterEnd_) {
        relaunchAfterEnd_ = false;
        QTimer::singleShot(0, this, &MainWindow::onSimulationStart);
    }
}

void MainWindow::onServeProcessEnded()
{
    if (awaitingServeExit_) {
        completeLiveSession();  // stopped, and its run folder written
        return;
    }
    if (connectingClient_) {
        // It ended while hello was on its way: no hello can come.
        connectingClient_->close();
        return;
    }
    if (!liveSessionActive()) return;
    // The process ended under a live session (a crash, stopped elsewhere, or
    // before our stop completed): no reply can come, so end the requests in
    // flight rather than wait for them.
    liveClient_->close();
    if (!liveEnding_) endLiveSession(tr("mime serve ended (see MIME Output)."));
}

// ---------------------------------------------------------------------------
// Run folders
// ---------------------------------------------------------------------------

void MainWindow::onOpenRunFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Run Folder"), runsDir(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) openRunFolder(dir);
}

bool MainWindow::openRunFolder(const QString& folder)
{
    if (liveSessionActive()) {
        QMessageBox::information(this, tr("Run folder"), tr("Stop the live session before opening a run folder."));
        return false;
    }
    std::unique_ptr<runs::RunFolder> run;
    try {
        run = std::make_unique<runs::RunFolder>(folder.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Not a run folder"), QString::fromUtf8(e.what()));
        return false;
    }

    // What to draw: its recording (played), or its experiment's stage.
    const QString recording = QString::fromStdString(run->recording());
    const QString stage = QString::fromStdString(run->stage());
    const QString open = !recording.isEmpty() ? recording : stage;
    if (open.isEmpty() || !openStage(open)) {
        QMessageBox::warning(this, tr("Run folder"),
            tr("The run folder has no recording or stage that opens:\n%1").arg(folder));
        return false;
    }
    runFolder_ = folder;
    // Its experiment is the run's: Launch runs it again, live.
    experimentDir_ = run->experimentFolder();
    if (runConfigPanel_) runConfigPanel_->setExperimentDir(experimentDir_);
    if (projectBrowser_) projectBrowser_->setExperimentDir(folder.toStdString());

    // The graph it ran, its settings, its outputs and MIME's claims.
    if (graphInspector_ && !run->topology().empty()) {
        std::ifstream in(run->topology());
        try {
            graphInspector_->loadFromJson(nlohmann::json::parse(in));
        } catch (const std::exception& e) {
            spdlog::warn("openRunFolder: topology.json not read: {}", e.what());
        }
    }
    if (liveInputsPanel_) {
        liveInputsPanel_->clearAll();
        std::ifstream lock(run->experimentFolder() + "/mime.lock");
        if (lock) {
            try {
                const auto j = nlohmann::json::parse(lock);
                if (j.contains("resolved") && j["resolved"].contains("entries")) {
                    liveInputsPanel_->showSettings(j["resolved"]["entries"]);
                }
            } catch (const std::exception&) {}
        }
    }
    try {
        plotsPanel_->showRun(run->recordedSeries(), run->duration());
    } catch (const std::exception& e) {
        plotsPanel_->showRun({}, run->duration());
        mimeConsole_->appendStderr((QStringLiteral("[run] outputs.npz not read: ") + e.what() + '\n').toUtf8());
    }
    claimsPanel_->showRun(*run);
    replayAction_->setEnabled(!run->commandsLog().empty() && !run->experimentFolder().empty());

    statusBar()->showMessage(tr("Run folder: %1 (%2 s%3)")
        .arg(QFileInfo(folder).fileName()).arg(run->duration())
        .arg(recording.isEmpty() ? tr(", no recording: its stage is shown") : QString()));
    if (!recording.isEmpty()) {
        requestVisualsView(folder, open, false);
    }
    return true;
}

void MainWindow::requestVisualsView(const QString& folder, const QString& opened, bool remoteSession)
{
    if (visualsProcess_) {
        visualsProcess_->kill();
        visualsProcess_->deleteLater();
        visualsProcess_ = nullptr;
    }
    visualsProcess_ = new QProcess(this);
    QProcess* process = visualsProcess_;
    // Still wanted when it answers: the same run folder open, or the same
    // live session.
    const quint64 session = liveSessionId_;
    auto wanted = [this, folder, remoteSession, session]() {
        return remoteSession ? session == liveSessionId_ && liveClient_ != nullptr : runFolder_ == folder;
    };
    connect(process, &QProcess::errorOccurred, this, [this, process, remoteSession, wanted](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        process->deleteLater();
        if (visualsProcess_ == process) visualsProcess_ = nullptr;
        if (remoteSession && wanted()) {
            mimeConsole_->appendStdout(tr("[session] No MIME on this machine (Settings → MIME command): the meshes "
                                          "MIME does not ship are drawn from this machine's asset cache, so the "
                                          "session draws as simple shapes\n").toUtf8());
        }
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, opened, remoteSession, wanted](int code, QProcess::ExitStatus status) {
        process->deleteLater();
        if (visualsProcess_ == process) visualsProcess_ = nullptr;
        // MIME refused or was stopped, or another folder or session since.
        if (status != QProcess::NormalExit || code != 0 || !wanted()) return;
        // Its notes, then the layer to open: the recording itself when nothing
        // is fetched, or a layer in MIME's cache that draws the meshes over it.
        const runs::VisualsView view = runs::parseVisualsView(process->readAllStandardOutput().toStdString());
        if (view.layer.empty()) return;
        const QString tag = remoteSession ? QStringLiteral("session") : QStringLiteral("visuals");
        for (const std::string& line : view.lines) {
            mimeConsole_->appendStdout(QStringLiteral("[%1] %2\n").arg(tag, QString::fromStdString(line)).toUtf8());
        }
        bool drawn = false;
        if (remoteSession) {
            // The overlay alone, over the stage the session's frames move.
            drawn = !view.overlay.empty() && QFileInfo::exists(QString::fromStdString(view.overlay))
                && sceneMgr_->setOverlayLayer(view.overlay);
        } else {
            const QString layer = QString::fromStdString(view.layer).trimmed();
            drawn = QFileInfo::exists(layer)
                && QFileInfo(layer).canonicalFilePath() != QFileInfo(opened).canonicalFilePath();
            if (drawn) {
                spdlog::info("openRunFolder: drawing MIME's visuals: {}", layer.toStdString());
                openStage(layer);
            }
        }
        showVisuals(drawn ? view.drawn : 0, view.missing, view.fetch, view.notice,
                    remoteSession ? tr("connect again") : tr("open the run folder again"), tag, false);
    });
    process->start(mimeCommand(), {QStringLiteral("visuals"), QStringLiteral("view"), folder});
}

void MainWindow::clearVisuals()
{
    if (noticeOverlay_) noticeOverlay_->removeNotice(QStringLiteral("visuals"));
    if (commandBar_) commandBar_->clear();
}

void MainWindow::showVisuals(long drawn, const std::map<std::string, long>& missing,
                             const std::map<std::string, std::string>& fetch, const std::vector<std::string>& notices,
                             const QString& again, const QString& tag, bool noticesInConsole)
{
    clearVisuals();
    auto say = [this, &tag](const QString& text) {
        mimeConsole_->appendStdout(QStringLiteral("[%1] %2\n").arg(tag, text).toUtf8());
        spdlog::info("visuals: {}", text.toStdString());
    };
    QStringList all;
    for (const auto& notice : notices) all << QString::fromStdString(notice);
    if (!missing.empty()) {
        QStringList sets;
        QStringList commands;
        for (const auto& [set, count] : missing) {
            sets << tr("%1 (%2 meshes)").arg(QString::fromStdString(set)).arg(count);
            // The command MIME gave for the set, exactly (its folder quoted
            // for a shell, by MIME): never one built here.
            if (const auto it = fetch.find(set); it != fetch.end() && !it->second.empty()) {
                const QString command = QString::fromStdString(it->second);
                if (!commands.contains(command)) commands << command;
            }
        }
        const QString lacks = sets.join(QStringLiteral(", "));
        if (commands.isEmpty()) {
            const QString text = tr("Drawn as simple shapes: MIME's asset cache lacks %1, and MIME named no "
                                    "command to fetch them (mime visuals view <the saved or run folder> says "
                                    "how).").arg(lacks);
            say(text);
            if (noticesInConsole) say(all.join(QLatin1Char('\n')));
            if (noticeOverlay_) {
                noticeOverlay_->addNotice(QStringLiteral("visuals"), viewport::NoticeOverlay::Severity::Info,
                                          tr("Visuals not fetched"), text + QLatin1Char('\n') + all.join(QLatin1Char('\n')));
            }
            return;
        }
        say(tr("Drawn as simple shapes: MIME's asset cache lacks %1. To draw them, run\n%2\nthen %3.")
                .arg(lacks, commands.join(QLatin1Char('\n')), again));
        if (noticesInConsole) say(all.join(QLatin1Char('\n')));
        if (commandBar_) {
            commandBar_->setCommands(tr("To draw the meshes MIME's asset cache lacks (%1), run this in a terminal, "
                                        "then %2:").arg(lacks, again), commands);
        }
        if (noticeOverlay_) {
            noticeOverlay_->addNotice(QStringLiteral("visuals"), viewport::NoticeOverlay::Severity::Info,
                tr("Visuals not fetched"),
                tr("Drawn as simple shapes: MIME's asset cache lacks %1. To draw them, run the command under the "
                   "viewport (Copy puts it on the clipboard), then %2.").arg(lacks, again)
                    + QLatin1Char('\n') + all.join(QLatin1Char('\n')));
        }
    } else if (drawn > 0) {
        // The meshes drawn are their makers' design: their credit stays in
        // view with them (each set's whole notice is in the console).
        QStringList credit;
        for (const auto& line : runs::noticeCredit(notices)) credit << QString::fromStdString(line);
        if (noticesInConsole) {
            say(tr("%1 meshes drawn from MIME's asset cache").arg(drawn));
            say(all.join(QLatin1Char('\n')));
        }
        if (noticeOverlay_ && !credit.isEmpty()) {
            noticeOverlay_->addNotice(QStringLiteral("visuals"), viewport::NoticeOverlay::Severity::Info,
                                      tr("Meshes from MIME's asset cache"), credit.join(QLatin1Char('\n')));
        }
    }
}

void MainWindow::onReplayRequested()
{
    if (runFolder_.isEmpty() || (replayProcess_ && replayProcess_->state() != QProcess::NotRunning)) return;
    runs::RunFolder run(runFolder_.toStdString());
    const QString experiment = QString::fromStdString(run.experimentFolder());
    const QString log = QString::fromStdString(run.commandsLog());
    if (experiment.isEmpty() || log.isEmpty()) return;
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmssZ"));
    const QString out = QDir(runsDir()).filePath(
        QStringLiteral("%1-replay-%2").arg(QString::fromStdString(run.experiment()), stamp));
    QDir().mkpath(runsDir());

    replayProcess_ = new QProcess(this);
    QProcess* process = replayProcess_;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains(QStringLiteral("JAX_PLATFORMS"))) env.insert(QStringLiteral("JAX_PLATFORMS"), QStringLiteral("cpu"));
    process->setProcessEnvironment(env);
    connect(process, &QProcess::readyReadStandardOutput, this,
            [this, process]() { mimeConsole_->appendStdout(process->readAllStandardOutput()); });
    connect(process, &QProcess::readyReadStandardError, this,
            [this, process]() { mimeConsole_->appendStderr(process->readAllStandardError()); });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, process, out](int code, QProcess::ExitStatus) {
        mimeConsole_->flush();
        process->deleteLater();
        replayProcess_ = nullptr;
        replayAction_->setEnabled(!runFolder_.isEmpty());
        if (code == 0 && runs::RunFolder::looksLikeRunFolder(out.toStdString())) {
            statusBar()->showMessage(tr("Replayed into %1").arg(out));
            openRunFolder(out);
        } else {
            statusBar()->showMessage(tr("The replay did not finish (see MIME Output)"));
            mimeConsole_->raise();
        }
    });
    const QStringList args = {QStringLiteral("run"), experiment, QStringLiteral("--commands"), log,
                              QStringLiteral("--out"), out};
    mimeConsole_->appendStdout(QStringLiteral("$ %1 %2\n").arg(mimeCommand(), args.join(' ')).toUtf8());
    replayAction_->setEnabled(false);
    statusBar()->showMessage(tr("Replaying the session into %1...").arg(out));
    process->start(mimeCommand(), args);
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
    simController_->setPaused(true);  // a live session: `pause`
    if (!liveSessionActive()) {
        timelinePanel_->pausePolling();  // a live session's step still sends frames
    }
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

    if (readyTimer_ && readyTimer_->isActive()) {  // stop a session still starting
        readyTimer_->stop();
        if (noticeOverlay_) noticeOverlay_->removeNotice(QStringLiteral("mime-serve"));
        if (serveProcess_) serveProcess_->stopAsync(0);  // not ready: nothing to finish
        runConfigPanel_->setLaunchEnabled(true);
        runConfigPanel_->setConnectionStatus(tr("Idle"));
        statusBar()->showMessage(tr("mime serve stopped before it was ready"));
        return;
    }
    if (connectingClient_) {  // stop a connection still waiting for hello
        cancelLiveConnect();
        statusBar()->showMessage(tr("Connection cancelled"));
        return;
    }
    if (liveSessionActive()) {
        endLiveSession(tr("Live session stopped."));  // returns at once; the end follows
        return;
    }

    paused_ = false;
    simController_->setPaused(false);
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

    // A live session's step and time, from its frames (each header carries
    // them); its state and activity come from replies. A frame past the
    // step shown means a chunk has been stepped, so the first has compiled
    // (frame 0, the starting state, may come while it compiles).
    if (liveSessionActive()) {
        if (frame.step > liveLine_.step && liveLine_.activity == "compiling") liveLine_.activity.clear();
        liveLine_.step = frame.step;
        liveLine_.time = t;
        showLiveLine();
    }
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
