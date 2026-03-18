#include "scene/scene_manager.h"
#include "scene/results_applicator.h"
#include "core/stability.h"
#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/primRange.h>
#endif

namespace microbotica::scene {

SceneManager::SceneManager(core::AuditLogger& logger, QObject* parent)
    : QObject(parent), logger_(&logger)
{
    MBCA_EXPERIMENTAL_WARN("SceneManager");
}

SceneManager::SceneManager(QObject* parent)
    : QObject(parent), logger_(&nullLogger_)
{
    MBCA_EXPERIMENTAL_WARN("SceneManager");
}

SceneManager::~SceneManager() = default;

bool SceneManager::loadScene(const std::string& filePath)
{
#ifdef MICROBOTICA_HAS_USD
    stage_ = UsdStage::Open(filePath);
    if (!stage_) {
        spdlog::error("SceneManager: Failed to open USD file: {}", filePath);
        return false;
    }

    // Three-layer composition stack:
    //   Root (session) layer — empty, used to compose sublayers
    //     Sublayer 0: Results layer (strongest — simulation output)
    //     Sublayer 1: Override layer (middle — user scripting writes)
    //     Sublayer 2: Base layer (weakest — original scene file)
    //
    // The stage's root layer becomes the session layer.
    // The file's content is accessed via its sublayer path.
    baseLayer_ = stage_->GetRootLayer();

    // Create a new anonymous root layer that composes everything
    auto sessionLayer = SdfLayer::CreateAnonymous("session");
    resultsLayer_ = SdfLayer::CreateAnonymous("results");
    overrideLayer_ = SdfLayer::CreateAnonymous("override");

    // Build sublayer stack: results (strongest) → override → base (weakest)
    sessionLayer->InsertSubLayerPath(baseLayer_->GetIdentifier());
    sessionLayer->InsertSubLayerPath(overrideLayer_->GetIdentifier(), 0);
    sessionLayer->InsertSubLayerPath(resultsLayer_->GetIdentifier(), 0);

    // Replace the stage's root layer with our session layer
    stage_ = UsdStage::Open(sessionLayer);

    applicator_ = std::make_unique<ResultsApplicator>(resultsLayer_, stage_);
    sceneLoaded_ = true;

    spdlog::info("SceneManager: Loaded scene {} with three-layer stack", filePath);
    logger_->logEvent("scene_load", {{"path", filePath}});

    sceneLoaded();
    return true;
#else
    spdlog::warn("SceneManager: USD not available, scene loading disabled");
    (void)filePath;
    return false;
#endif
}

void SceneManager::applyResultFrame(const core::ResultFrame& frame,
                                     const core::PhysicsConfig& config)
{
#ifdef MICROBOTICA_HAS_USD
    if (!applicator_) return;
    applicator_->apply(frame, config);
    stageChanged();
#else
    (void)frame;
    (void)config;
#endif
}

void SceneManager::crashRecovery()
{
#ifdef MICROBOTICA_HAS_USD
    if (resultsLayer_) {
        resultsLayer_->Clear();
        spdlog::info("SceneManager: Crash recovery — results layer cleared");
        stageChanged();
    }
#endif
    backendCrashed();
}

bool SceneManager::saveOverrideLayer(const std::string& path)
{
#ifdef MICROBOTICA_HAS_USD
    if (!overrideLayer_) return false;
    return overrideLayer_->Export(path);
#else
    (void)path;
    return false;
#endif
}

std::vector<std::string> SceneManager::primPaths() const
{
    std::vector<std::string> paths;
#ifdef MICROBOTICA_HAS_USD
    if (stage_) {
        for (const auto& prim : stage_->Traverse()) {
            paths.push_back(prim.GetPath().GetString());
        }
    }
#endif
    return paths;
}

} // namespace microbotica::scene
