#pragma once

#ifdef MICROBOTICA_HAS_USD

#include <cstdint>
#include <memory>
#include <string>

#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/glf/simpleMaterial.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

#include "core/component_meta.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace microbotica::viewport {

/// MICROROBOTICA's viewport camera: Z-up, the eye on a sphere around a target in the XY plane.
struct OrbitView {
    double distance = 1.0;    ///< eye to target, in stage units
    double azimuth = 0.0;     ///< radians, in the XY plane from +X
    double elevation = 0.0;   ///< radians, from the XY plane toward +Z
    double panX = 0.0;        ///< the target's x
    double panY = 0.0;        ///< the target's y
    double panZ = 0.0;        ///< the target's z (the app's camera keeps it 0; a test frames a thing with it)
    double fovDegrees = 45.0;

    GfVec3d target() const { return GfVec3d(panX, panY, panZ); }
    GfVec3d eye() const;
};

/// Hydra Storm drawing a USD stage into a framebuffer of the OpenGL context current on the calling thread: the
/// engine, the camera's matrices with clip planes fitted to the stage's bounds, and the lights. It holds no Qt, so the
/// viewport window and the headless screenshot test (an EGL context on Mesa) draw through the same code.
///
/// MBCA-COMP-032
class StormRenderer {
public:
    static const core::ComponentMeta& meta() {
        static const core::ComponentMeta m{
            .component_id      = "MBCA-COMP-032",
            .component_version = "1.0.0",
            .stability         = core::StabilityLevel::Experimental,
            .description       = "Hydra Storm render path (engine, camera, lights) shared by the viewport "
                                 "window and the headless screenshot tests.",
            .preconditions     = {"An OpenGL 4.5 Compatibility context is current on the calling thread"},
            .postconditions    = {"render() draws the stage at the given time code into the given framebuffer"},
            .invariants        = {"The stage is only read, never written"},
            .design_rationale  = "One render path for the app and its screenshot tests, so the tests check the "
                                 "app's own drawing; no Qt, so it runs on a headless EGL context.",
            .assumptions       = {"Mesa (llvmpipe or a GPU driver) or another driver provides GL 4.5 Compatibility"},
            .limitations       = {"Scene bounds are computed once per stage, at its start, middle and end time codes",
                                  "Translucent materials need a multisample framebuffer (alpha-to-coverage)"},
            .validated_regimes = {{"OpenGL context version (Compatibility profile)", 4.5, 4.5, "",
                                   "Mesa 25.2.8 llvmpipe, OpenUSD 24.08: MBCA-VER-011, MBCA-VER-012"}},
            .hazard_hints      = {
                "A prim outside the bounds computed at load can be clipped by the fitted far plane.",
            },
            .references        = {},
            .deprecation_notice = std::nullopt,
        };
        return m;
    }

    /// @post No engine exists until the first render() with a stage.
    StormRenderer();
    ~StormRenderer();
    StormRenderer(const StormRenderer&) = delete;
    StormRenderer& operator=(const StormRenderer&) = delete;

    /// The stage to draw (null for none).
    /// @post sceneCenter() and sceneRadius() describe its bounds; the engine is recreated at the next render(). Computes its world bounds over its time range (start, middle, end, so a
    /// recording is enclosed throughout); the engine is created anew at the next render.
    void setStage(UsdStageRefPtr stage);
    UsdStageRefPtr stage() const { return stage_; }

    /// Centre and radius of the stage's bounds, computed by setStage (radius 0 when unknown).
    GfVec3d sceneCenter() const { return sceneCenter_; }
    double sceneRadius() const { return sceneRadius_; }

    /// Draw the stage at `time` (UsdTimeCode::Default() for a live stage) from `view` into framebuffer `fbo` of
    /// `widthPx` x `heightPx` pixels. Returns true when Storm reports the image converged (it usually does after one
    /// call; a few more right after the stage is set, while shaders compile). False with no stage or an empty size.
    /// @pre An OpenGL 4.5 Compatibility context is current, and `fbo` is a framebuffer of it.
    /// @note Translucent materials draw see-through only when `fbo` is multisampled.
    bool render(const OrbitView& view, UsdTimeCode time, int widthPx, int heightPx, std::uint32_t fbo);

    /// The Hydra renderer plugin in use (HdStormRendererPlugin), empty before the first render.
    std::string rendererId() const;

private:
    void initLights();

    UsdStageRefPtr stage_;
    std::unique_ptr<UsdImagingGLEngine> engine_;
    GfVec3d sceneCenter_ = GfVec3d(0.0);
    double sceneRadius_ = 0.0;

    bool lightsReady_ = false;
    GlfSimpleLightVector lights_;
    GlfSimpleMaterial material_;
    GfVec4f sceneAmbient_ = GfVec4f(0.0f);
};

} // namespace microbotica::viewport

#endif // MICROBOTICA_HAS_USD
