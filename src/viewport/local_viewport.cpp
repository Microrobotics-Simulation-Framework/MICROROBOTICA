#include "viewport/local_viewport.h"
#include "viewport/camera_controller.h"

#include "core/stability.h"

#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "core/profiler.h"

#ifdef MICROBOTICA_HAS_USD
#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/tokens.h>
#endif

namespace microbotica::viewport {

LocalViewport::LocalViewport(QWindow* parent)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, parent)
{
    MBCA_EXPERIMENTAL_WARN("LocalViewport");

    // Request a Compatibility Profile context. USD's HgiGL (Storm) uses
    // GL state (glPushAttrib etc.) that only exists in Compatibility Profile.
    // NVIDIA 535.x exposes GL 4.6 in Compat but only 4.5 in Core Profile.
    // Without this, Storm spams "invalid enum" errors on every frame.
    QSurfaceFormat fmt;
    fmt.setVersion(4, 6);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    // 4x MSAA. Also required by params.enableSampleAlphaToCoverage (set in
    // paintGL): Storm fakes order-independent transparency by turning fragment
    // alpha into multisample coverage, so the translucent vessel / tube
    // materials only render see-through when a multisample buffer exists.
    fmt.setSamples(4);
    // Swap interval 0 — do NOT vsync this window. It is a native window
    // composited by the desktop compositor, which already presents at the
    // display refresh. Vsyncing here too creates a double-sync: this window's
    // buffer swap blocks on vblank and contends with the compositor's own
    // present, which hitches the whole desktop — worst during continuous
    // repaints such as zooming. Let the compositor be the sole vsync.
    fmt.setSwapInterval(0);
    setFormat(fmt);

    cameraController_ = new CameraController(this);
    installEventFilter(cameraController_);

    // Repaint on demand when the camera moves. The camera controller cannot
    // call update() on us directly — this is a native QWindow, not a QWidget —
    // so it signals instead.
    connect(cameraController_, &CameraController::cameraChanged,
            this, [this]() { update(); });

    // Continuous-rendering timer, used only for live simulation (the USD stage
    // mutates every step with nothing else triggering a repaint). Started and
    // stopped by setContinuousRendering(); it stays idle for recordings, which
    // repaint on demand and otherwise leave the GPU alone.
    renderTimer_ = new QTimer(this);
    renderTimer_->setInterval(16);  // ~60 fps
    connect(renderTimer_, &QTimer::timeout, this, [this]() { update(); });

#ifdef MICROBOTICA_HAS_USD
    usdAvailable_ = true;
#else
    usdAvailable_ = false;
#endif
}

void LocalViewport::setContinuousRendering(bool enabled) {
    // The viewport renders continuously whenever a stage is loaded (the timer
    // is started by setStage), so this flag is informational only.
    continuousRendering_ = enabled;
}

void LocalViewport::setTimeCode(double t) {
    useCustomTimeCode_ = true;
    customTime_ = t;
    update();
}

void LocalViewport::resetTimeCode() {
    useCustomTimeCode_ = false;
    customTime_ = 0.0;
    update();
}

LocalViewport::~LocalViewport() = default;

#ifdef MICROBOTICA_HAS_USD
void LocalViewport::setStage(UsdStageRefPtr stage)
{
    stage_ = stage;
    engine_.reset();

    // Compute the scene's world-space bounds once, here, so paintGL can fit the
    // near/far clip planes to it (see the depth-precision note there). Union
    // start / middle / end so an animated recording is fully enclosed.
    sceneCenter_ = GfVec3d(0.0);
    sceneRadius_ = 0.0;
    if (stage_) {
        UsdGeomBBoxCache bboxCache(
            UsdTimeCode::Default(),
            {UsdGeomTokens->default_, UsdGeomTokens->render},
            /*useExtentsHint*/ true);
        GfRange3d range;
        const double t0 = stage_->GetStartTimeCode();
        const double t1 = stage_->GetEndTimeCode();
        for (double t : {t0, 0.5 * (t0 + t1), t1}) {
            bboxCache.SetTime(UsdTimeCode(t));
            range.UnionWith(bboxCache.ComputeWorldBound(
                stage_->GetPseudoRoot()).ComputeAlignedRange());
        }
        if (!range.IsEmpty()) {
            sceneCenter_ = range.GetMidpoint();
            sceneRadius_ = 0.5 * range.GetSize().GetLength();
        }
    }

    // Storm warms up over the first frames after the engine is recreated
    // (shader compiles, texture loads); allow a bounded burst of repaints to
    // converge that. See the convergence pump in paintGL().
    convergencePumpsLeft_ = 180;

    // Drive the viewport from the repaint timer the whole time a stage is
    // loaded. On-demand repainting proved unreliable for this embedded native
    // window (paintGL simply stopped being called), so render continuously
    // instead — the same configuration that worked after the native-window
    // switch. The engine is (re)created on the next paintGL.
    if (stage_) {
        renderTimer_->start();
    } else {
        renderTimer_->stop();
    }
    update();
}
#endif

void LocalViewport::initializeGL()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
    f->glEnable(GL_DEPTH_TEST);

    // Log GL diagnostics so we can tell what renderer is active
    auto* ctx = QOpenGLContext::currentContext();
    if (ctx && ctx->isValid()) {
        const char* vendor = reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(f->glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
        auto fmt = ctx->format();
        int major = fmt.majorVersion();
        int minor = fmt.minorVersion();
        const char* profile = (fmt.profile() == QSurfaceFormat::CompatibilityProfile)
                              ? "Compatibility" : "Core";
        spdlog::info("LocalViewport GL: vendor={}, renderer={}, version={} "
                     "(context: {}.{} {} Profile)",
                     vendor ? vendor : "unknown",
                     renderer ? renderer : "unknown",
                     version ? version : "unknown",
                     major, minor, profile);

        // Storm (HgiGL) requires OpenGL 4.6 Compatibility Profile.
        // If < 4.6, mark as failed — don't try Render().
        if (major < 4 || (major == 4 && minor < 6)) {
            spdlog::error("LocalViewport: GL {}.{} {} Profile is below the "
                          "4.6 minimum required by Hydra/Storm. "
                          "Falling back to Software viewport.",
                          major, minor, profile);
            renderFailed_ = true;
            Q_EMIT renderFailed();
            return;
        }
    }
}

void LocalViewport::paintGL()
{
    MBCA_PROFILE_SCOPE("paintGL");
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // If a previous render produced GL errors, don't keep trying — show message.
    if (renderFailed_) {
        QPainter painter(this);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Sans", 12));
        painter.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter,
            QStringLiteral("Hydra/Storm render failed (GL errors)\n"
                           "Scene loaded OK — viewport will switch to Software mode"));
        painter.end();
        return;
    }

#ifdef MICROBOTICA_HAS_USD
    if (!stage_) {
        return;
    }

    if (!engine_) {
        engine_ = std::make_unique<UsdImagingGLEngine>();
    }

    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0) {
        return;
    }

    const double aspect = static_cast<double>(w) / static_cast<double>(h);

    const double dist = cameraController_->orbitDistance();
    const double azim = cameraController_->azimuth();
    const double elev = cameraController_->elevation();

    // Camera in spherical coordinates (Z-up, matching USD upAxis = "Z").
    // Azimuth: angle in XY plane from +X axis. Mouse-right = positive azimuth.
    // Elevation: angle from XY plane toward +Z. Mouse-up = positive elevation.
    const double cosElev = std::cos(elev);
    GfVec3d eye(dist * cosElev * std::cos(azim),
                dist * cosElev * std::sin(azim),
                dist * std::sin(elev));

    GfVec3d target(cameraController_->panX(),
                    cameraController_->panY(),
                    0.0);
    eye += target;

    GfVec3d up(0.0, 0.0, 1.0);

    // Fit the clip planes to the scene bounds rather than using a fixed,
    // enormous near/far ratio (which would wreck depth-buffer precision).
    // Falls back to a distance-based estimate if the bounds are unavailable.
    double nearPlane, farPlane;
    if (sceneRadius_ > 0.0) {
        const double camToScene = (eye - sceneCenter_).GetLength();
        farPlane  = camToScene + sceneRadius_ * 1.25;
        nearPlane = std::max(camToScene - sceneRadius_ * 1.25, farPlane * 1e-4);
    } else {
        nearPlane = std::max(dist * 0.001, 1e-7);
        farPlane  = dist * 10.0;
    }

    // Build view and projection matrices directly.
    // SetLookAt(eye, target, up) produces the world-to-camera transform.
    GfMatrix4d viewMatrix;
    viewMatrix.SetLookAt(eye, target, up);

    GfFrustum frustum;
    frustum.SetPerspective(45.0, aspect, nearPlane, farPlane);
    GfMatrix4d projMatrix = frustum.ComputeProjectionMatrix();

    UsdImagingGLRenderParams params;
    params.frame = useCustomTimeCode_ ? UsdTimeCode(customTime_) : UsdTimeCode::Default();
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    params.enableLighting = true;
    params.enableSceneMaterials = true;
    params.enableSampleAlphaToCoverage = true;  // OIT approximation for glass/transparent materials
    params.complexity = 1.2f;

    engine_->SetCameraState(viewMatrix, projMatrix);

    f->glViewport(0, 0, w * devicePixelRatio(), h * devicePixelRatio());
    engine_->SetRenderViewport(GfVec4d(0, 0, w * devicePixelRatio(),
                                            h * devicePixelRatio()));

    // Lighting: initialize once, then only update key light position per frame.
    if (!lightingInitialized_) {
        GlfSimpleLight keyLight(GfVec4f(0, 0, 0, 1));
        keyLight.SetDiffuse(GfVec4f(0.9f, 0.9f, 0.9f, 1.0f));
        keyLight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
        keyLight.SetSpecular(GfVec4f(0.4f, 0.4f, 0.4f, 1.0f));

        GlfSimpleLight fillLight(GfVec4f(0, 0, 0, 1));
        fillLight.SetDiffuse(GfVec4f(0.3f, 0.3f, 0.35f, 1.0f));
        fillLight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
        fillLight.SetSpecular(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));

        lights_ = { keyLight, fillLight };

        material_.SetAmbient(GfVec4f(0.1f, 0.1f, 0.1f, 1.0f));
        material_.SetDiffuse(GfVec4f(0.8f, 0.8f, 0.8f, 1.0f));
        material_.SetSpecular(GfVec4f(0.3f, 0.3f, 0.3f, 1.0f));
        material_.SetShininess(32.0f);

        sceneAmbient_ = GfVec4f(0.08f, 0.08f, 0.1f, 1.0f);
        lightingInitialized_ = true;
    }

    // Only the key light position changes per frame (tracks camera).
    // Fill light tracks opposite side.
    lights_[0].SetPosition(GfVec4f(
        static_cast<float>(eye[0]), static_cast<float>(eye[1]),
        static_cast<float>(eye[2]), 1.0f));
    lights_[1].SetPosition(GfVec4f(
        static_cast<float>(-eye[0]), static_cast<float>(eye[1] + dist * 0.5),
        static_cast<float>(-eye[2]), 1.0f));

    engine_->SetLightingState(lights_, material_, sceneAmbient_);

    // Present Storm's result into the framebuffer this QOpenGLWindow draws to.
    // In NoPartialUpdate mode that is framebuffer 0 (the window surface), which
    // is also Storm's default — but binding it explicitly keeps this correct if
    // the update mode ever changes to an FBO-backed one. Re-set every frame
    // because the framebuffer can be recreated on resize.
    engine_->SetPresentationOutput(
        HgiTokens->OpenGL,
        VtValue(static_cast<uint32_t>(defaultFramebufferObject())));

    {
        MBCA_PROFILE_SCOPE("hydra_render");
        engine_->Render(stage_->GetPseudoRoot(), params);
    }

    // Storm normally converges in a single Render(); the exception is warm-up
    // right after a stage loads (shader compiles, texture loads). Pump a
    // bounded number of repaints to get through that — bounded so a backend
    // that never reports convergence cannot spin the render loop forever.
    if (!engine_->IsConverged() && convergencePumpsLeft_ > 0) {
        --convergencePumpsLeft_;
        update();
    }

#else
    // USD not available — paint a placeholder message.
    QPainter painter(this);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Sans", 14));
    painter.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter,
                     QStringLiteral("OpenUSD not available"));
    painter.end();
#endif
}

void LocalViewport::resizeGL(int w, int h)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glViewport(0, 0, w * devicePixelRatio(), h * devicePixelRatio());
}

} // namespace microbotica::viewport
