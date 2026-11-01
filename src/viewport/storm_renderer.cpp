#include "viewport/storm_renderer.h"

#ifdef MICROBOTICA_HAS_USD

#include <algorithm>
#include <cmath>

#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usdImaging/usdImagingGL/renderParams.h>

#include "core/profiler.h"
#include "core/stability.h"

namespace microbotica::viewport {

GfVec3d OrbitView::eye() const {
    // Spherical coordinates about the target, Z up (USD upAxis = "Z"): azimuth in the XY plane from +X, elevation
    // from the XY plane toward +Z.
    const double cosElev = std::cos(elevation);
    return target() + GfVec3d(distance * cosElev * std::cos(azimuth),
                              distance * cosElev * std::sin(azimuth),
                              distance * std::sin(elevation));
}

StormRenderer::StormRenderer() {
    MBCA_EXPERIMENTAL_WARN("StormRenderer");
}
StormRenderer::~StormRenderer() = default;

void StormRenderer::setStage(UsdStageRefPtr stage) {
    stage_ = std::move(stage);
    engine_.reset();
    sceneCenter_ = GfVec3d(0.0);
    sceneRadius_ = 0.0;
    if (!stage_) {
        return;
    }
    // World bounds once, here, so render() can fit the near and far planes to them (depth precision). The union of
    // start, middle and end, so an animated recording is enclosed.
    UsdGeomBBoxCache bboxCache(UsdTimeCode::Default(), {UsdGeomTokens->default_, UsdGeomTokens->render},
                               /*useExtentsHint*/ true);
    GfRange3d range;
    const double t0 = stage_->GetStartTimeCode();
    const double t1 = stage_->GetEndTimeCode();
    for (double t : {t0, 0.5 * (t0 + t1), t1}) {
        bboxCache.SetTime(UsdTimeCode(t));
        range.UnionWith(bboxCache.ComputeWorldBound(stage_->GetPseudoRoot()).ComputeAlignedRange());
    }
    if (!range.IsEmpty()) {
        sceneCenter_ = range.GetMidpoint();
        sceneRadius_ = 0.5 * range.GetSize().GetLength();
    }
}

void StormRenderer::initLights() {
    GlfSimpleLight keyLight(GfVec4f(0, 0, 0, 1));
    keyLight.SetDiffuse(GfVec4f(0.9f, 0.9f, 0.9f, 1.0f));
    keyLight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
    keyLight.SetSpecular(GfVec4f(0.4f, 0.4f, 0.4f, 1.0f));

    GlfSimpleLight fillLight(GfVec4f(0, 0, 0, 1));
    fillLight.SetDiffuse(GfVec4f(0.3f, 0.3f, 0.35f, 1.0f));
    fillLight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
    fillLight.SetSpecular(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));

    lights_ = {keyLight, fillLight};

    material_.SetAmbient(GfVec4f(0.1f, 0.1f, 0.1f, 1.0f));
    material_.SetDiffuse(GfVec4f(0.8f, 0.8f, 0.8f, 1.0f));
    material_.SetSpecular(GfVec4f(0.3f, 0.3f, 0.3f, 1.0f));
    material_.SetShininess(32.0f);

    sceneAmbient_ = GfVec4f(0.08f, 0.08f, 0.1f, 1.0f);
    lightsReady_ = true;
}

bool StormRenderer::render(const OrbitView& view, UsdTimeCode time, int widthPx, int heightPx, std::uint32_t fbo) {
    if (!stage_ || widthPx <= 0 || heightPx <= 0) {
        return false;
    }
    if (!engine_) {
        engine_ = std::make_unique<UsdImagingGLEngine>();
    }

    const GfVec3d eye = view.eye();
    const GfVec3d target = view.target();

    // Fit the clip planes to the scene's bounds rather than a fixed, enormous near/far ratio (which would wreck
    // depth-buffer precision); a distance-based estimate when the bounds are unknown.
    double nearPlane = 0.0;
    double farPlane = 0.0;
    if (sceneRadius_ > 0.0) {
        const double camToScene = (eye - sceneCenter_).GetLength();
        farPlane = camToScene + sceneRadius_ * 1.25;
        nearPlane = std::max(camToScene - sceneRadius_ * 1.25, farPlane * 1e-4);
    } else {
        nearPlane = std::max(view.distance * 0.001, 1e-7);
        farPlane = view.distance * 10.0;
    }

    GfMatrix4d viewMatrix;
    viewMatrix.SetLookAt(eye, target, GfVec3d(0.0, 0.0, 1.0));
    GfFrustum frustum;
    frustum.SetPerspective(view.fovDegrees, static_cast<double>(widthPx) / static_cast<double>(heightPx), nearPlane,
                           farPlane);
    engine_->SetCameraState(viewMatrix, frustum.ComputeProjectionMatrix());
    engine_->SetRenderViewport(GfVec4d(0, 0, widthPx, heightPx));

    // The key light at the eye, the fill opposite it.
    if (!lightsReady_) {
        initLights();
    }
    lights_[0].SetPosition(GfVec4f(static_cast<float>(eye[0]), static_cast<float>(eye[1]),
                                   static_cast<float>(eye[2]), 1.0f));
    lights_[1].SetPosition(GfVec4f(static_cast<float>(-eye[0]), static_cast<float>(eye[1] + view.distance * 0.5),
                                   static_cast<float>(-eye[2]), 1.0f));
    engine_->SetLightingState(lights_, material_, sceneAmbient_);

    // Present into the caller's framebuffer, set every call: a window's framebuffer can be recreated on resize.
    engine_->SetPresentationOutput(HgiTokens->OpenGL, VtValue(static_cast<std::uint32_t>(fbo)));

    UsdImagingGLRenderParams params;
    params.frame = time;
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    params.enableLighting = true;
    params.enableSceneMaterials = true;
    params.enableSampleAlphaToCoverage = true;  // translucent tube and vessel, with a multisample buffer
    params.complexity = 1.2f;
    {
        MBCA_PROFILE_SCOPE("hydra_render");
        engine_->Render(stage_->GetPseudoRoot(), params);
    }
    return engine_->IsConverged();
}

std::string StormRenderer::rendererId() const {
    return engine_ ? engine_->GetCurrentRendererId().GetString() : std::string();
}

} // namespace microbotica::viewport

#endif // MICROBOTICA_HAS_USD
