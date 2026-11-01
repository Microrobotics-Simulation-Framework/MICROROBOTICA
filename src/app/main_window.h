#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class QAction;
class QProcess;
class QTemporaryDir;
class QTimer;

#include "core/stability.h"
#include "core/audit_logger.h"
#include "core/physics_config.h"
#include "scene/scene_manager.h"
#include "scene/prim_selection.h"
#include "simulation/simulation_controller.h"
#include "scripting/scripting_engine.h"

namespace microbotica::viewport {
class ViewportWidget;
class CameraToolbar;
class NoticeOverlay;
} // namespace microbotica::viewport

namespace microbotica::panels {
class SceneHierarchyPanel;
class PropertyPanel;
class TimelinePanel;
class ConsoleWidget;
class MimeConsolePanel;
class RunConfigPanel;
class ProjectBrowserPanel;
class GraphInspectorPanel;
class ScriptEditorPanel;
class LiveInputsPanel;
class PlotsPanel;
class ClaimsPanel;
} // namespace microbotica::panels

namespace microbotica::experiment {
class ExperimentRunner;
} // namespace microbotica::experiment

namespace microbotica::live {
class LiveClient;
class BindingFrames;
struct Hello;
struct Reply;
} // namespace microbotica::live

namespace microbotica::app {

/// Main application window with dock layout.
///
/// Owns all core services (SceneManager, SimulationController,
/// ScriptingEngine, PrimSelection) and all dock panels.
///
/// Dock layout:
///   Left:   SceneHierarchyPanel (top), PropertyPanel (bottom)
///   Right:  RunConfigPanel (top), ProjectBrowserPanel (bottom)
///   Bottom: TimelinePanel, ConsoleWidget, MimeConsolePanel
///   Center: ViewportWidget
///   Top toolbar: SimulationToolbar
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Open a path as File's actions do: a MIME 0.3 saved experiment folder,
    /// or a USD file (a recording plays). False, with a message, otherwise.
    bool openPath(const QString& path);

    /// Launch, as the Run Configuration panel's button does.
    void launch() { onSimulationStart(); }

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onFileOpen();
    void onFileOpenExperiment();
    void onFileClose();
    void onSimulationStart();
    void onSimulationPause();
    void onSimulationStop();
    void onAbout();
    void onSettings();
    void onResetLayout();
    void onFrameReady(const microbotica::core::ResultFrame& frame);
    void onBackendCrashed(const QString& reason);

private:
    void createMenuBar();
    void createStatusBar();
    void createDockWidgets();
    void createToolBars();
    void wireSignals();
    void saveLayout();
    void restoreLayout();

    /// Open a MIME 0.3 saved experiment folder: load its stage
    /// (scene/scene.usda), remember it for Launch, and mirror it into the
    /// panels. Returns false (with a message) for any other folder.
    bool initExperiment(const QString& dir);

    /// Open a USD file as the scene: leave playback of the previous one,
    /// load it in the three-layer stack, switch the viewport to Storm, and
    /// enter playback mode when it is a time-sampled recording.
    bool openStage(const QString& filePath);

    /// Pick the compute backend for the selected mode and launch physics.
    void finishSimulationStart();

    // ── Live sessions (MIME 0.3, mime.live/1) ──────────────────────────────
    /// Local: spawn `mime serve <saved folder>` and connect once its ready
    /// file appears.
    void startLocalSession();
    /// Connect to a session (Local, once ready; Manual, at the panel's
    /// endpoints): hello, the stage, the panels, then start it.
    void connectLiveSession(const QString& commands, const QString& frames);
    /// What hello says, shown: the stage (and MIME's visuals overlay), the
    /// graph, the live inputs. False when the session's stage is not here.
    bool applyHello(const live::Hello& hello);
    /// A reply of the session, on the UI thread: a refusal goes to the MIME
    /// console in its shape; the status to the run panel. While the first
    /// chunk compiles or a pause is pending, `status` is polled.
    void onLiveReply(const live::Reply& reply);
    /// The run panel's status line: the state, what the session is doing and
    /// what is pending from replies, the step and time from frames.
    void showLiveLine();
    /// Tear the session down: the client, the panels, the MIME process.
    void endLiveSession(const QString& why);
    /// The `mime serve` process ended (stopped, or crashed).
    void onServeProcessEnded();
    void onStepRequested();
    void onRecordToggled(bool on);
    bool liveSessionActive() const;
    // ── Run folders (MIME 0.3): playback, outputs, claims, replay ──────────
    /// Open a run folder: its recording (or its stage), its experiment, its
    /// topology, its recorded series (plots) and MIME's claims about it.
    bool openRunFolder(const QString& folder);
    void onOpenRunFolder();
    /// Replay the open run folder's live session (`mime run <experiment>
    /// --commands commands.jsonl`) into a new run folder, then open it.
    void onReplayRequested();
    /// `mime visuals view <run folder>`: the layer that draws MIME's meshes
    /// over the recording, opened in its place when it differs.
    void requestVisualsView(const QString& runFolder, const QString& opened);

    /// Settings → MIME command (MICROROBOTICA_MIME overrides it).
    QString mimeCommand() const;
    /// Settings → Run folders.
    QString runsDir() const;

    /// Read the persisted notice-overlay preferences from QSettings and
    /// apply them to noticeOverlay_. Called on startup and after the
    /// Settings dialog is accepted.
    void applyNoticeSettings();

    // Core services
    core::NullAuditLogger auditLogger_;
    std::unique_ptr<scene::SceneManager> sceneMgr_;
    std::unique_ptr<scene::PrimSelection> primSelection_;
    std::unique_ptr<simulation::SimulationController> simController_;
    std::unique_ptr<scripting::ScriptingEngine> scriptEngine_;

    // The MIME 0.3 saved experiment opened (File → Open Saved Experiment);
    // empty when none is.
    std::string experimentDir_;

    // The live session: `mime serve` (Local), its client, its binding.
    std::unique_ptr<experiment::ExperimentRunner> serveProcess_;
    std::unique_ptr<QTemporaryDir> readyDir_;
    QTimer* readyTimer_ = nullptr;
    std::shared_ptr<live::LiveClient> liveClient_;
    std::shared_ptr<const live::BindingFrames> liveFrames_;
    nlohmann::json pendingSet_;   // the live inputs sent, until set's reply
    // The session's status line: state, activity and pending from replies,
    // step and time from frame headers (MIME's client note, entries 18, 21).
    struct LiveLine {
        std::string state;
        std::string activity;
        std::vector<std::string> pending;
        std::uint64_t step = 0;
        double time = 0.0;
        bool recording = false;
    } liveLine_;
    QTimer* liveStatusPoll_ = nullptr;  // status, while compiling or a pause is pending
    QString liveLineLogged_;      // the status line's last state, activity and pending, as logged
    QString liveOut_;             // where `mime serve --out` writes (Local): a pending stop's run folder
    QString sessionRunFolder_;    // the run folder this session's stop named
    bool liveStopPending_ = false;  // the stop was taken while a chunk ran
    QString lastRunFolder_;       // the run folder the last session wrote
    QString runFolder_;           // the run folder open (File → Open Run Folder)
    QAction* openLastRunAction_ = nullptr;
    QAction* replayAction_ = nullptr;
    QProcess* replayProcess_ = nullptr;
    QProcess* visualsProcess_ = nullptr;
    QAction* stepAction_ = nullptr;
    QAction* recordAction_ = nullptr;

    // Central widget
    viewport::ViewportWidget* viewportWidget_ = nullptr;
    viewport::NoticeOverlay* noticeOverlay_ = nullptr;

    // Toolbars
    viewport::CameraToolbar* cameraToolbar_ = nullptr;
    bool fullscreen_ = false;
    bool paused_ = false;
    bool playbackMode_ = false;

    // Dock panels
    panels::SceneHierarchyPanel* hierarchyPanel_ = nullptr;
    panels::PropertyPanel* propertyPanel_ = nullptr;
    panels::TimelinePanel* timelinePanel_ = nullptr;
    panels::ConsoleWidget* consoleWidget_ = nullptr;
    panels::MimeConsolePanel* mimeConsole_ = nullptr;
    panels::RunConfigPanel* runConfigPanel_ = nullptr;
    panels::ProjectBrowserPanel* projectBrowser_ = nullptr;
    panels::GraphInspectorPanel* graphInspector_ = nullptr;
    panels::ScriptEditorPanel* scriptEditor_ = nullptr;
    panels::LiveInputsPanel* liveInputsPanel_ = nullptr;
    panels::PlotsPanel* plotsPanel_ = nullptr;
    panels::ClaimsPanel* claimsPanel_ = nullptr;

    // Status bar widgets
    QLabel* simTimeLabel_ = nullptr;

    // Factory layout for reset
    QByteArray factoryLayout_;
    QByteArray factoryGeometry_;
};

} // namespace microbotica::app
