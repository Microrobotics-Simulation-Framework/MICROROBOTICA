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

#include "viewport/gl_requirements.h"

namespace microbotica::viewport {

LocalViewport::LocalViewport(int samples, QWindow* parent)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, parent)
{
    MBCA_EXPERIMENTAL_WARN("LocalViewport");

    // Request a Compatibility Profile context. USD's HgiGL (Storm) uses
    // GL state (glPushAttrib etc.) that only exists in Compatibility Profile;
    // without it Storm spams "invalid enum" errors on every frame. 4.5 is what
    // Storm needs and what Mesa gives on every driver, its software rasteriser
    // (llvmpipe) included; the Intel GPU through Mesa gives 4.6.
    QSurfaceFormat fmt;
    fmt.setVersion(kStormGlMajor, kStormGlMinor);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    // 4x MSAA by default (viewportSamples()). Also required by
    // params.enableSampleAlphaToCoverage (StormRenderer): Storm fakes
    // order-independent transparency by turning fragment alpha into
    // multisample coverage, so the translucent vessel / tube materials only
    // render see-through when a multisample buffer exists.
    fmt.setSamples(samples);
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
    // The renderer computes the stage's bounds (to fit the clip planes) and
    // recreates its engine on the next paintGL.
    storm_.setStage(stage);

    // Storm warms up over the first frames after the engine is recreated
    // (shader compiles, texture loads); allow a bounded burst of repaints to
    // converge that. See the convergence pump in paintGL().
    convergencePumpsLeft_ = 180;

    // Drive the viewport from the repaint timer the whole time a stage is
    // loaded. On-demand repainting proved unreliable for this embedded native
    // window (paintGL simply stopped being called), so render continuously
    // instead — the same configuration that worked after the native-window
    // switch.
    if (stage) {
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
        if (renderer && isSoftwareRasteriser(renderer)) {
            spdlog::info("LocalViewport: drawing on Mesa's software rasteriser "
                         "({}x MSAA; MICROROBOTICA_MSAA=0 trades the translucent "
                         "tube for speed on a large window)", fmt.samples());
        }

        // Storm (HgiGL) requires OpenGL 4.5 Compatibility Profile.
        // Below that, mark as failed — don't try Render().
        if (!meetsStormMinimum(major, minor)) {
            spdlog::error("LocalViewport: GL {}.{} {} Profile is below the "
                          "{} minimum required by Hydra/Storm. "
                          "Falling back to Software viewport.",
                          major, minor, profile, stormMinimumText());
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
    if (!storm_.stage()) {
        return;
    }

    const int w = static_cast<int>(width() * devicePixelRatio());
    const int h = static_cast<int>(height() * devicePixelRatio());
    if (w <= 0 || h <= 0) {
        return;
    }

    OrbitView view;
    view.distance = cameraController_->orbitDistance();
    view.azimuth = cameraController_->azimuth();
    view.elevation = cameraController_->elevation();
    view.panX = cameraController_->panX();
    view.panY = cameraController_->panY();

    f->glViewport(0, 0, w, h);

    // Present Storm's result into the framebuffer this QOpenGLWindow draws to.
    // In NoPartialUpdate mode that is framebuffer 0 (the window surface), which
    // is also Storm's default — but passing it explicitly keeps this correct if
    // the update mode ever changes to an FBO-backed one.
    const bool converged = storm_.render(
        view,
        useCustomTimeCode_ ? UsdTimeCode(customTime_) : UsdTimeCode::Default(),
        w, h, static_cast<std::uint32_t>(defaultFramebufferObject()));

    // Storm normally converges in a single Render(); the exception is warm-up
    // right after a stage loads (shader compiles, texture loads). Pump a
    // bounded number of repaints to get through that — bounded so a backend
    // that never reports convergence cannot spin the render loop forever.
    if (!converged && convergencePumpsLeft_ > 0) {
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
