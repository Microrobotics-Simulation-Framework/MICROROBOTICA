#include "viewport/viewport_widget.h"
#include "viewport/local_viewport.h"
#include "viewport/software_viewport.h"

#include "core/stability.h"
#include <QOpenGLContext>
#include <spdlog/spdlog.h>

namespace microbotica::viewport {

ViewportWidget::ViewportWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    MBCA_EXPERIMENTAL_WARN("ViewportWidget");

    softwareViewport_ = new SoftwareViewport(this);
    addWidget(softwareViewport_);

    // Start with SoftwareViewport only. LocalViewport (QOpenGLWidget) is
    // created lazily on first setRenderMode(LocalHydra) call. This avoids
    // "failed to create drawable" in Docker/headless environments where
    // QOpenGLWidget construction triggers an unusable GLX drawable.
    setCurrentWidget(softwareViewport_);
    currentMode_ = core::RenderMode::Software;
}

ViewportWidget::~ViewportWidget() = default;

#ifdef MICROBOTICA_HAS_USD
void ViewportWidget::setStage(UsdStageRefPtr stage)
{
    pendingStage_ = stage;
    if (localViewport_) {
        localViewport_->setStage(stage);
    }
    softwareViewport_->setStageLoaded(stage != nullptr);
}
#else
void ViewportWidget::setStage(void* /*stage*/)
{
    // USD not available; software viewport remains in placeholder state.
}
#endif

void ViewportWidget::setRenderMode(core::RenderMode mode)
{
    if (mode == currentMode_) {
        return;
    }

    switch (mode) {
    case core::RenderMode::LocalHydra: {
        // Lazily create LocalViewport on first request
        if (!localViewport_) {
            // Probe for a usable GL 4.6 Compatibility context before creating
            // QOpenGLWidget. If the driver can't provide it, stay on Software.
            // This prevents "failed to create drawable" crashes on Mesa/headless.
            QSurfaceFormat probeFmt;
            probeFmt.setVersion(4, 6);
            probeFmt.setProfile(QSurfaceFormat::CompatibilityProfile);

            QOpenGLContext testCtx;
            testCtx.setFormat(probeFmt);
            bool ctxOk = testCtx.create();

            if (ctxOk) {
                auto actual = testCtx.format();
                int maj = actual.majorVersion();
                int min = actual.minorVersion();
                if (maj < 4 || (maj == 4 && min < 6)) {
                    ctxOk = false;
                    spdlog::warn("ViewportWidget: GL probe got {}.{} {} — need 4.6 Compat",
                                 maj, min,
                                 actual.profile() == QSurfaceFormat::CompatibilityProfile
                                     ? "Compatibility" : "Core");
                }
            }

            if (!ctxOk) {
                spdlog::warn("ViewportWidget: No usable GL 4.6 Compatibility context — "
                             "falling back to Software viewport");
                softwareViewport_->setStatusMessage(
                    "3D Viewport — Software Mode\n\n"
                    "Hydra/Storm requires OpenGL 4.6 (GPU).\n"
                    "Scene is loaded and simulation will run — use the\n"
                    "scene hierarchy panel to inspect prims.\n\n"
                    "For GPU rendering, run with:\n"
                    "  docker run --gpus all ...");
                return;
            }

            localViewport_ = new LocalViewport(this);
            addWidget(localViewport_);
            spdlog::info("ViewportWidget: LocalViewport created on demand");

            // If Hydra fails on the first frame, fall back to Software
            connect(localViewport_, &LocalViewport::renderFailed,
                    this, [this]() {
                spdlog::warn("ViewportWidget: Hydra render failed — switching to Software viewport");
                softwareViewport_->setStatusMessage(
                    "3D Viewport — Software Mode\n\n"
                    "Hydra/Storm could not initialize on this GL context.\n"
                    "Scene is loaded and simulation will run — use the\n"
                    "scene hierarchy panel to inspect prims.\n\n"
                    "Try updating your GPU driver or running with --gpus all.");
                setCurrentWidget(softwareViewport_);
                currentMode_ = core::RenderMode::Software;
            });

#ifdef MICROBOTICA_HAS_USD
            // Forward any stage that was set before LocalViewport existed
            if (pendingStage_) {
                localViewport_->setStage(pendingStage_);
            }
#endif
        }
        setCurrentWidget(localViewport_);
        break;
    }
    case core::RenderMode::Software:
    case core::RenderMode::CloudStream:
        setCurrentWidget(softwareViewport_);
        break;
    }

    currentMode_ = mode;
    Q_EMIT renderModeChanged(mode);
}

void ViewportWidget::setContinuousRendering(bool enabled) {
    if (localViewport_) {
        localViewport_->setContinuousRendering(enabled);
    }
}

} // namespace microbotica::viewport
