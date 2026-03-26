#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QByteArray>
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
class CameraToolbar;
} // namespace microbotica::viewport

namespace microbotica::panels {
class SceneHierarchyPanel;
class PropertyPanel;
class TimelinePanel;
class ConsoleWidget;
class SimulationToolbar;
class MimeConsolePanel;
class RunConfigPanel;
class SkyPilotMonitorPanel;
class ProjectBrowserPanel;
class GraphInspectorPanel;
class ScriptEditorPanel;
class ParameterPanel;
} // namespace microbotica::panels

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

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void onFileOpen();
    void onFileClose();
    void onSimulationStart();
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

    // Core services
    core::NullAuditLogger auditLogger_;
    std::unique_ptr<scene::SceneManager> sceneMgr_;
    std::unique_ptr<scene::PrimSelection> primSelection_;
    std::unique_ptr<simulation::SimulationController> simController_;
    std::unique_ptr<scripting::ScriptingEngine> scriptEngine_;

    // Central widget
    viewport::ViewportWidget* viewportWidget_ = nullptr;

    // Toolbars
    panels::SimulationToolbar* simToolbar_ = nullptr;
    viewport::CameraToolbar* cameraToolbar_ = nullptr;

    // Dock panels
    panels::SceneHierarchyPanel* hierarchyPanel_ = nullptr;
    panels::PropertyPanel* propertyPanel_ = nullptr;
    panels::TimelinePanel* timelinePanel_ = nullptr;
    panels::ConsoleWidget* consoleWidget_ = nullptr;
    panels::MimeConsolePanel* mimeConsole_ = nullptr;
    panels::RunConfigPanel* runConfigPanel_ = nullptr;
    panels::SkyPilotMonitorPanel* skypilotPanel_ = nullptr;
    panels::ProjectBrowserPanel* projectBrowser_ = nullptr;
    panels::GraphInspectorPanel* graphInspector_ = nullptr;
    panels::ScriptEditorPanel* scriptEditor_ = nullptr;
    panels::ParameterPanel* parameterPanel_ = nullptr;

    // Status bar widgets
    QLabel* simTimeLabel_ = nullptr;

    // Factory layout for reset
    QByteArray factoryLayout_;
    QByteArray factoryGeometry_;
};

} // namespace microbotica::app
