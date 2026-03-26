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
            // Check for a usable OpenGL context before creating QOpenGLWidget.
            // In Docker/headless without GPU passthrough, QOpenGLWidget creation
            // triggers "failed to create drawable" and may crash.
            QOpenGLContext testCtx;
            if (!testCtx.create()) {
                spdlog::warn("ViewportWidget: No usable OpenGL context — "
                             "falling back to Software viewport. "
                             "Run with GPU access (--gpus all) or Mesa "
                             "(LIBGL_ALWAYS_SOFTWARE=1) for Hydra rendering.");
                softwareViewport_->setStatusMessage(
                    "Hydra/Storm unavailable — no OpenGL context.\n"
                    "Run Docker with --gpus all or set LIBGL_ALWAYS_SOFTWARE=1");
                // Stay on Software mode
                return;
            }

            localViewport_ = new LocalViewport(this);
            addWidget(localViewport_);
            spdlog::info("ViewportWidget: LocalViewport created on demand");

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

} // namespace microbotica::viewport
