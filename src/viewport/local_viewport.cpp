#include "viewport/local_viewport.h"
#include "viewport/camera_controller.h"

#include "core/stability.h"

#include <QPainter>
#include <QOpenGLFunctions>
#include <spdlog/spdlog.h>

#ifdef MICROBOTICA_HAS_USD
#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/usd/usd/prim.h>
#endif

namespace microbotica::viewport {

LocalViewport::LocalViewport(QWidget* parent)
    : QOpenGLWidget(parent)
{
    MBCA_EXPERIMENTAL_WARN("LocalViewport");

    cameraController_ = new CameraController(this);
    installEventFilter(cameraController_);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

#ifdef MICROBOTICA_HAS_USD
    usdAvailable_ = true;
#else
    usdAvailable_ = false;
#endif
}

LocalViewport::~LocalViewport() = default;

#ifdef MICROBOTICA_HAS_USD
void LocalViewport::setStage(UsdStageRefPtr stage)
{
    stage_ = stage;
    engine_.reset();

    if (stage_) {
        // Engine will be (re)created on the next paintGL when GL context is current.
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
        spdlog::info("LocalViewport GL: vendor={}, renderer={}, version={}",
                     vendor ? vendor : "unknown",
                     renderer ? renderer : "unknown",
                     version ? version : "unknown");
    }
}

void LocalViewport::paintGL()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // If a previous render produced GL errors, don't keep trying — show message.
    if (renderFailed_) {
        QPainter painter(this);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Sans", 12));
        painter.drawText(rect(), Qt::AlignCenter,
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
        // Drain any pre-existing GL errors before engine creation
        while (f->glGetError() != GL_NO_ERROR) {}

        engine_ = std::make_unique<UsdImagingGLEngine>();

        // Check if engine creation itself produced GL errors
        GLenum err = f->glGetError();
        if (err != GL_NO_ERROR) {
            spdlog::warn("LocalViewport: GL error 0x{:04X} during UsdImagingGLEngine creation "
                         "— Storm may not work on this GL context", err);
        }
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

    const double cosElev = std::cos(elev);
    GfVec3d eye(dist * cosElev * std::sin(azim),
                dist * std::sin(elev),
                dist * cosElev * std::cos(azim));

    GfVec3d target = GfVec3d(cameraController_->panX(),
                              cameraController_->panY(),
                              0.0);
    eye += target;

    GfVec3d up(0.0, 1.0, 0.0);

    GfFrustum frustum;
    frustum.SetPerspective(45.0, aspect, 0.1, 10000.0);
    frustum.SetPosition(eye);
    frustum.SetRotation(
        GfRotation(GfMatrix4d().SetLookAt(eye, target, up).ExtractRotation()));

    UsdImagingGLRenderParams params;
    params.frame = UsdTimeCode::Default();
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;

    engine_->SetCameraState(
        frustum.ComputeViewMatrix(),
        frustum.ComputeProjectionMatrix());

    f->glViewport(0, 0, w * devicePixelRatio(), h * devicePixelRatio());
    engine_->SetRenderViewport(GfVec4d(0, 0, w * devicePixelRatio(),
                                            h * devicePixelRatio()));

    // Drain GL errors before render so we only catch new ones
    while (f->glGetError() != GL_NO_ERROR) {}

    engine_->Render(stage_->GetPseudoRoot(), params);

    // After the first render, check if HgiGL produced errors.
    // USD's HgiGL uses glGetError internally and logs "Runtime Error" but
    // doesn't throw — it just spams errors every frame. We detect this on
    // the first frame and fall back to Software mode.
    if (!firstRenderDone_) {
        firstRenderDone_ = true;
        GLenum postErr = f->glGetError();
        if (postErr != GL_NO_ERROR) {
            spdlog::error("LocalViewport: GL error 0x{:04X} after first Render() — "
                          "Hydra/Storm is not compatible with this GL context. "
                          "Falling back to Software viewport.", postErr);
            renderFailed_ = true;
            engine_.reset();
            Q_EMIT renderFailed();
            return;
        }
        spdlog::info("LocalViewport: First render succeeded — Hydra/Storm active");
    }

#else
    // USD not available — paint a placeholder message.
    QPainter painter(this);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Sans", 14));
    painter.drawText(rect(), Qt::AlignCenter,
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
