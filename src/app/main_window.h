#pragma once

#include <QMainWindow>
#include <QLabel>
#include <memory>

#include "core/stability.h"
#include "core/audit_logger.h"
#include "core/physics_config.h"
#include "scene/scene_manager.h"
#include "scene/prim_selection.h"
#include "simulation/simulation_controller.h"
#include "scripting/scripting_engine.h"

namespace microbotica::viewport {
class ViewportWidget;
} // namespace microbotica::viewport

namespace microbotica::panels {
class SceneHierarchyPanel;
class PropertyPanel;
class TimelinePanel;
class ConsoleWidget;
} // namespace microbotica::panels

namespace microbotica::app {

/// Main application window with dock layout.
///
/// Owns all core services (SceneManager, SimulationController,
/// ScriptingEngine, PrimSelection) and all dock panels.
///
/// Dock layout:
///   Left:   SceneHierarchyPanel (top), PropertyPanel (bottom)
///   Bottom: TimelinePanel, ConsoleWidget
///   Center: ViewportWidget
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onFileOpen();
    void onFileClose();
    void onSimulationStart();
    void onSimulationStop();
    void onAbout();
    void onFrameReady(const microbotica::core::ResultFrame& frame);
    void onBackendCrashed(const QString& reason);

private:
    void createMenuBar();
    void createStatusBar();
    void createDockWidgets();
    void wireSignals();

    // Core services
    core::NullAuditLogger auditLogger_;
    std::unique_ptr<scene::SceneManager> sceneMgr_;
    std::unique_ptr<scene::PrimSelection> primSelection_;
    std::unique_ptr<simulation::SimulationController> simController_;
    std::unique_ptr<scripting::ScriptingEngine> scriptEngine_;

    // Central widget
    viewport::ViewportWidget* viewportWidget_ = nullptr;

    // Dock panels
    panels::SceneHierarchyPanel* hierarchyPanel_ = nullptr;
    panels::PropertyPanel* propertyPanel_ = nullptr;
    panels::TimelinePanel* timelinePanel_ = nullptr;
    panels::ConsoleWidget* consoleWidget_ = nullptr;

    // Status bar widgets
    QLabel* simTimeLabel_ = nullptr;
};

} // namespace microbotica::app
