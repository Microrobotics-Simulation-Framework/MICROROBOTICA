#pragma once

#include <string>
#include <memory>
#include <functional>
#include <QObject>

#include "core/component_meta.h"
#include "core/result_frame.h"
#include "core/physics_config.h"
#include "core/audit_logger.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
PXR_NAMESPACE_USING_DIRECTIVE
#endif

namespace microbotica::scene {

class ResultsApplicator;

/// Manages the USD three-layer composition stack.
///
/// MBCA-COMP-010
///
/// Three layers:
///   Base (immutable after load) → Override (scripting writes) → Results (simulation output)
class SceneManager : public QObject {
    Q_OBJECT

public:
    explicit SceneManager(core::AuditLogger& logger, QObject* parent = nullptr);
    SceneManager(QObject* parent = nullptr);
    ~SceneManager() override;

    static const core::ComponentMeta& interfaceMeta() {
        static const core::ComponentMeta meta{
            .component_id      = "MBCA-COMP-010",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "USD three-layer scene manager with layer separation enforcement.",
            .preconditions     = {"USD file exists and is readable"},
            .postconditions    = {"After loadScene(), three layers are established",
                                  "Base layer is never modified after load"},
            .invariants        = {"Results layer is never persisted to disk",
                                  "Base layer content is immutable after load"},
            .design_rationale  = "Three-layer USD composition ensures simulation output "
                                 "cannot contaminate the original scene geometry.",
            .assumptions       = {"Only one scene is loaded at a time"},
            .limitations       = {"Crash recovery clears entire results layer — see MBCA-ANO-003",
                                  "No undo/redo for override layer modifications"},
            .validated_regimes = {},
            .hazard_hints      = {
                "Partial results layer writes on crash — see MBCA-ANO-003.",
                "Base layer mutation would corrupt anatomical reference data.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return meta;
    }

    /// Load a USD scene file and establish the three-layer stack.
    /// @throws std::runtime_error if file cannot be opened.
    bool loadScene(const std::string& filePath);

    /// Apply a ResultFrame to the results layer.
    void applyResultFrame(const core::ResultFrame& frame,
                          const core::PhysicsConfig& config);

    /// Clear the results layer (crash recovery).
    void crashRecovery();

    /// Save the override layer to a sidecar file.
    bool saveOverrideLayer(const std::string& path);

    /// Check if a scene is loaded.
    bool isLoaded() const { return sceneLoaded_; }

    /// Get prim paths in the current stage.
    std::vector<std::string> primPaths() const;

#ifdef MICROBOTICA_HAS_USD
    /// Get the USD stage (for viewport rendering).
    UsdStageRefPtr stage() const { return stage_; }

    /// Get layer handles for testing.
    SdfLayerRefPtr baseLayer() const { return baseLayer_; }
    SdfLayerRefPtr overrideLayer() const { return overrideLayer_; }
    SdfLayerRefPtr resultsLayer() const { return resultsLayer_; }
#endif

    /// Close the current scene and release the stage.
    void closeScene();

    /// Scene generation counter — incremented on load/close.
    /// Used by ResultsApplicator to invalidate cached xform op handles.
    uint64_t sceneGeneration() const { return sceneGeneration_; }

signals:
    void sceneLoaded();
    void sceneClosed();
    void stageChanged();
    void primChanged(const std::string& primPath);
    void backendCrashed();

private:
    core::AuditLogger* logger_ = nullptr;
    core::NullAuditLogger nullLogger_;
    bool sceneLoaded_ = false;
    uint64_t sceneGeneration_ = 0;
    std::unique_ptr<ResultsApplicator> applicator_;

#ifdef MICROBOTICA_HAS_USD
    UsdStageRefPtr stage_;
    SdfLayerRefPtr baseLayer_;
    SdfLayerRefPtr overrideLayer_;
    SdfLayerRefPtr resultsLayer_;
#endif
};

} // namespace microbotica::scene
