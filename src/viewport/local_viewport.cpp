#include "viewport/local_viewport.h"
#include "viewport/camera_controller.h"

#include "core/stability.h"

#include <QPainter>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
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

    // Request a Compatibility Profile context. USD's HgiGL (Storm) uses
    // GL state (glPushAttrib etc.) that only exists in Compatibility Profile.
    // NVIDIA 535.x exposes GL 4.6 in Compat but only 4.5 in Core Profile.
    // Without this, Storm spams "invalid enum" errors on every frame.
    QSurfaceFormat fmt;
    fmt.setVersion(4, 6);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    setFormat(fmt);

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

    engine_->Render(stage_->GetPseudoRoot(), params);

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
