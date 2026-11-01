#include "scene/scene_manager.h"
#include "scene/results_applicator.h"
#include "core/stability.h"
#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_USD
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/sdf/schema.h>   // SdfFieldKeys (FramesPerSecond)
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/tokens.h>
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

    // Read animation metadata from the original stage BEFORE replacing
    // it with the session layer (the session layer is anonymous and won't
    // have the time metadata authored on the base layer).
    hasAnimation_ = stage_->HasAuthoredTimeCodeRange();
    if (hasAnimation_) {
        startTimeCode_ = stage_->GetStartTimeCode();
        endTimeCode_ = stage_->GetEndTimeCode();
        timeCodesPerSecond_ = stage_->GetTimeCodesPerSecond();
    } else {
        startTimeCode_ = 0.0;
        endTimeCode_ = 0.0;
        timeCodesPerSecond_ = 24.0;
    }

    // Create a new anonymous root layer that composes everything
    auto sessionLayer = SdfLayer::CreateAnonymous("session");
    resultsLayer_ = SdfLayer::CreateAnonymous("results");
    overrideLayer_ = SdfLayer::CreateAnonymous("override");

    // Build sublayer stack: results (strongest) → override → base (weakest)
    sessionLayer->InsertSubLayerPath(baseLayer_->GetIdentifier());
    sessionLayer->InsertSubLayerPath(overrideLayer_->GetIdentifier(), 0);
    sessionLayer->InsertSubLayerPath(resultsLayer_->GetIdentifier(), 0);

    // Mirror the base layer's time-code metadata onto the session layer
    // so the *root* of the composed stage carries the same start/end/fps
    // the recording authored.  USD time-code metadata is sticky to the
    // root layer (it does NOT inherit from sublayers like prim/attribute
    // data does).  Without this, an anonymous session layer falls back
    // to USD's defaults — TimeCodesPerSecond=24, StartTimeCode=0,
    // EndTimeCode=0 — and Hydra evaluates time-sampled attributes
    // through that lens.  Net effect: a recording authored at fps>24
    // appears to "freeze at frame (24/fps_authored) × N" because past
    // that, Hydra is being asked to evaluate attributes outside the
    // stage's reported duration and returns the last cached state.
    // Verified against three test clips:
    //   150f @ 100 fps stops at  36 = 24 × 1.5 s
    //   300f @  30 fps stops at 240 = 24 ×  10 s
    //   600f @  30 fps stops at 480 = 24 ×  20 s
    //   300f @  60 fps stops at 120 = 24 ×   5 s
    if (hasAnimation_) {
        sessionLayer->SetStartTimeCode(startTimeCode_);
        sessionLayer->SetEndTimeCode(endTimeCode_);
        sessionLayer->SetTimeCodesPerSecond(timeCodesPerSecond_);
        // FramesPerSecond is a separate metadatum used by some viewers
        // for playback rate; mirror it from the source too.  Falls back
        // to TimeCodesPerSecond if the source didn't author it.
        const double fps = stage_->HasAuthoredMetadata(
                                SdfFieldKeys->FramesPerSecond)
            ? stage_->GetFramesPerSecond() : timeCodesPerSecond_;
        sessionLayer->SetFramesPerSecond(fps);
    }

    // The same holds for the stage's units and up axis: metadata of its root
    // layer, which the anonymous session layer would otherwise lack, so the
    // composed stage would read USD's fallbacks (centimetres, Y up). MIME's
    // stages and recordings author both; a live session's positions, in
    // metres, are converted by metersPerUnit.
    for (const TfToken& key : {UsdGeomTokens->metersPerUnit, UsdGeomTokens->upAxis}) {
        if (stage_->HasAuthoredMetadata(key)) {
            VtValue value;
            stage_->GetMetadata(key, &value);
            sessionLayer->GetPseudoRoot()->SetInfo(key, value);
        }
    }

    // Replace the stage's root layer with our session layer
    stage_ = UsdStage::Open(sessionLayer);

    applicator_ = std::make_unique<ResultsApplicator>(resultsLayer_, stage_);
    overlayPath_.clear();
    filePath_ = filePath;
    sceneLoaded_ = true;
    ++sceneGeneration_;

    if (hasAnimation_) {
        spdlog::info("SceneManager: Loaded scene {} with three-layer stack "
                     "(animation: frames {}-{} at {} fps)",
                     filePath, startTimeCode_, endTimeCode_, timeCodesPerSecond_);
    } else {
        spdlog::info("SceneManager: Loaded scene {} with three-layer stack", filePath);
    }
    logger_->logEvent("scene_load", {{"path", filePath}});

    Q_EMIT sceneLoaded();
    if (hasAnimation_) {
        Q_EMIT animationDetected(startTimeCode_, endTimeCode_, timeCodesPerSecond_);
    }
    return true;
#else
    spdlog::warn("SceneManager: USD not available, scene loading disabled");
    (void)filePath;
    return false;
#endif
}

void SceneManager::closeScene() {
#ifdef MICROBOTICA_HAS_USD
    applicator_.reset();
    stage_.Reset();
    baseLayer_.Reset();
    overrideLayer_.Reset();
    resultsLayer_.Reset();
#endif
    sceneLoaded_ = false;
    hasAnimation_ = false;
    overlayPath_.clear();
    filePath_.clear();
    ++sceneGeneration_;
    Q_EMIT sceneClosed();
}

bool SceneManager::setOverlayLayer(const std::string& path)
{
#ifdef MICROBOTICA_HAS_USD
    if (!stage_ || !baseLayer_) return false;
    auto overlay = SdfLayer::FindOrOpen(path);
    if (!overlay) {
        spdlog::warn("SceneManager: cannot open the overlay layer {}", path);
        return false;
    }
    auto session = stage_->GetRootLayer();
    auto indexOf = [&session](const std::string& id) {
        const std::vector<std::string> paths = session->GetSubLayerPaths();
        for (size_t i = 0; i < paths.size(); ++i) {
            if (paths[i] == id) return static_cast<int>(i);
        }
        return -1;
    };
    if (!overlayPath_.empty()) {
        const int old = indexOf(overlayPath_);
        if (old >= 0) session->RemoveSubLayerPath(old);
    }
    // [results, override, overlay, base]: stronger than the stage, weaker
    // than the user's overrides and the simulation's results.
    session->InsertSubLayerPath(overlay->GetIdentifier(), indexOf(baseLayer_->GetIdentifier()));
    overlayPath_ = overlay->GetIdentifier();
    ++sceneGeneration_;  // prims under the overlay may be new: drop cached xform ops
    spdlog::info("SceneManager: overlay {} added over the stage", overlayPath_);
    stageChanged();
    return true;
#else
    (void)path;
    return false;
#endif
}

double SceneManager::metersPerUnit() const
{
#ifdef MICROBOTICA_HAS_USD
    if (stage_) {
        return UsdGeomGetStageMetersPerUnit(stage_);
    }
#endif
    return 1.0;
}

std::unordered_map<std::string, double> SceneManager::vectorMetresPerUnit() const
{
    std::unordered_map<std::string, double> out;
#ifdef MICROBOTICA_HAS_USD
    if (!stage_) return out;
    static const TfToken name("mime:vector:metres_per_unit");
    for (const auto& prim : stage_->Traverse()) {
        const auto attr = prim.GetAttribute(name);
        double value = 0.0;
        if (attr && attr.Get(&value)) {
            out[prim.GetPath().GetString()] = value;
        }
    }
#endif
    return out;
}

void SceneManager::applyResultFrame(const core::ResultFrame& frame,
                                     const core::PhysicsConfig& config)
{
#ifdef MICROBOTICA_HAS_USD
    if (!applicator_) return;
    applicator_->setSceneGeneration(sceneGeneration_);
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
